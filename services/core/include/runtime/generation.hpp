#pragma once
#include "runtime/studio.hpp"
#include "runtime/generation_budget.hpp"
namespace runtime::generation {
using application::Json;using application::ensure;
struct Options { bool enabled=false;std::string model;std::uint64_t input_rate=0,output_rate=0,ceiling=250000,monthly_limit=100000000;Json schema; };
class Engine {
  std::string database_;Options options_;std::function<Json(const Json&)> dispatch_;
  Json receipt(Database& db,const studio::Principal& p,const std::string& id) const {
    auto r=db.query("SELECT state,result::text,charged_microusd::text,created_at::text FROM application_generations WHERE workspace_id=$1 AND principal_id=$2 AND id=$3",{p.workspace,p.id,id});ensure(r.size()==1,"not_found","Generation not found.");
    return {{"id",id},{"state",r.get(0,0)},{"result",application::read_document(r.get(0,1))},{"accounted_microusd",r.get(0,2)},{"created_at",r.get(0,3)}};
  }
public:
  Engine(std::string database,Options options,std::function<Json(const Json&)> dispatch):database_(std::move(database)),options_(std::move(options)),dispatch_(std::move(dispatch)){}
  bool configured() const {return options_.enabled && !options_.model.empty() && options_.input_rate>0 && options_.output_rate>0 && static_cast<bool>(dispatch_);}
  studio::Reply handle(const studio::Principal& p,const studio::Request& request) {
    ensure(p.can_build,"forbidden","Builder permission required.");const auto path=studio::parts(request.path);
    if(path.size()==5 && path[4]=="config" && request.method=="GET")return {{{"configured",configured()},{"model",options_.model},{"ceiling_microusd",std::to_string(options_.ceiling)},{"live_provider_verified",false}},std::nullopt,200};
    if(path.size()==5 && request.method=="GET"){ensure(storage::valid_key(path[4]),"not_found","Generation not found.");Database db(database_);db.begin(p.workspace);db.query("UPDATE application_generations SET state='unknown' WHERE workspace_id=$1 AND principal_id=$2 AND id=$3 AND state='dispatched' AND created_at<now()-interval '3 minutes'",{p.workspace,p.id,path[4]});auto result=receipt(db,p,path[4]);db.commit();return {result,std::nullopt,200};}
    ensure(path.size()==4 && request.method=="POST","not_found","Endpoint not found.");ensure(configured(),"planner_not_configured","Configure the planner model, credentials and explicit pricing limits. No fixed application is substituted.");
    auto input=application::read_document(request.body);application::keys(input,{"brief","application_id"},{"brief"});const auto brief=input.at("brief").get<std::string>();ensure(!brief.empty() && brief.size()<=12000,"document_limit","Application brief must contain 1–12000 UTF-8 bytes.");
    const auto request_key=studio::key(request),hash=runtime::sha256(input.dump());std::string id;Json provider_request;
    {
      Database db(database_);db.begin(p.workspace);db.query("SELECT pg_advisory_xact_lock(hashtextextended($1,1))",{p.workspace});
      auto prior=db.query("SELECT id,input_hash FROM application_generations WHERE workspace_id=$1 AND principal_id=$2 AND request_key=$3",{p.workspace,p.id,request_key});
      if(prior.size()){ensure(prior.get(0,1)==hash,"idempotency_conflict","Generation key was already used for another request.");auto out=receipt(db,p,prior.get(0,0));db.commit();return {out,std::nullopt,200};}
      id=runtime::random_id();provider_request={{"model",options_.model},{"brief",brief},{"schema",options_.schema},{"max_output_tokens",8192},{"dispatch_id",id}};
      if(input.contains("application_id")){
        const auto app=input.at("application_id").get<std::string>();ensure(storage::valid_key(app),"not_found","Application not found.");auto current=db.query("SELECT v.spec::text,a.head_revision::text FROM applications a JOIN application_versions v ON v.workspace_id=a.workspace_id AND v.app_id=a.id AND v.revision=a.head_revision WHERE a.workspace_id=$1 AND a.id=$2 AND a.creator_id=$3",{p.workspace,app,p.id});ensure(current.size()==1,"not_found","Application not found.");provider_request["previous_spec"]=application::read_document(current.get(0,0));provider_request["base_revision"]=std::stoi(current.get(0,1));provider_request["application_id"]=app;
      }
      const auto amount=reserve(provider_request.dump().size()+4096,options_.input_rate,options_.output_rate,options_.ceiling);
      auto used=db.query("SELECT COALESCE(sum(charged_microusd),0)::text,count(*)::text,count(*) FILTER(WHERE state='dispatched')::text FROM application_generations WHERE workspace_id=$1 AND created_at>=date_trunc('month',now())",{p.workspace});
      ensure(std::stoull(used.get(0,0))+amount<=options_.monthly_limit && std::stoull(used.get(0,1))<1000 && std::stoull(used.get(0,2))<2,"budget_limit","Monthly allowance or concurrent generation limit reached, including unresolved reservations.");
      db.query("INSERT INTO application_generations(workspace_id,id,principal_id,request_key,input_hash,state,request,charged_microusd) VALUES($1,$2,$3,$4,$5,'dispatched',$6::jsonb,$7::bigint)",{p.workspace,id,p.id,request_key,hash,provider_request.dump(),std::to_string(amount)});db.commit();
    }
    Json result;std::string state="unknown";std::optional<std::uint64_t> charge;
    try {
      auto provider=dispatch_(provider_request);application::keys(provider,{"spec_text","input_tokens","output_tokens","model","provider_id"},{"spec_text","input_tokens","output_tokens","model","provider_id"});
      ensure(provider.at("input_tokens").is_number_unsigned() && provider.at("output_tokens").is_number_unsigned(),"unknown_usage","Missing token usage.");
      if(provider.at("model")==options_.model)charge=cost(provider.at("input_tokens").get<std::uint64_t>(),options_.input_rate)+cost(provider.at("output_tokens").get<std::uint64_t>(),options_.output_rate);
      result={{"provider_id",provider.at("provider_id")},{"model",provider.at("model")},{"input_tokens",provider.at("input_tokens")},{"output_tokens",provider.at("output_tokens")},{"base_revision",provider_request.value("base_revision",0)},{"application_id",provider_request.value("application_id",std::string{})},{"usage_settled",charge.has_value()}};
      try{const auto spec_json=application::read_document(provider.at("spec_text").get<std::string>());const auto spec=application::spec_from_json(spec_json);if(provider_request.contains("previous_spec"))application::compatible_upgrade(application::spec_from_json(provider_request.at("previous_spec")),spec);result["spec"]=spec_json;result["capabilities"]=application::capability_report(spec,application::builtins());state="complete";}
      catch(const std::exception&){state="invalid";result["error"]="The returned application failed structural or migration validation. No application was published and no simpler template was substituted.";}
    }catch(const std::exception&){result={{"error","Provider outcome or usage is uncertain. Reservation retained; this request will not be automatically resent."}};}
    Database db(database_);db.begin(p.workspace);
    if(charge)db.query("UPDATE application_generations SET state=$4,result=$5::jsonb,charged_microusd=$6::bigint WHERE workspace_id=$1 AND principal_id=$2 AND id=$3",{p.workspace,p.id,id,state,result.dump(),std::to_string(*charge)});
    else db.query("UPDATE application_generations SET state=$4,result=$5::jsonb WHERE workspace_id=$1 AND principal_id=$2 AND id=$3",{p.workspace,p.id,id,state,result.dump()});
    auto out=receipt(db,p,id);db.commit();return {out,std::nullopt,200};
  }
};
}
