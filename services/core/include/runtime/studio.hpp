#pragma once
#include "runtime/application_json.hpp"
#include "runtime/storage.hpp"
#include "runtime/platform.hpp"
#include <sstream>

namespace runtime::studio {
using application::Json;
using application::ensure;
struct Principal { std::string workspace, id; bool can_build=false; };
struct Request {
  std::string method,path,body,idempotency_key;
  std::map<std::string,std::string> query;
  std::string parameter(const std::string& key,std::string fallback={}) const { auto it=query.find(key);return it==query.end()?fallback:it->second; }
};
struct Reply { Json document; std::optional<std::string> bytes; int status=200; };
inline std::string key(const Request& request) {
  const auto& value=request.idempotency_key;
  ensure(value.size()>=16 && value.size()<=128 && value.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_")==std::string::npos,"invalid_key","A 16–128 character Idempotency-Key is required."); return value;
}
inline std::vector<std::string> parts(const std::string& path) {
  std::vector<std::string> result; std::istringstream stream(path);std::string part;
  while(std::getline(stream,part,'/')) { if(!part.empty())result.push_back(part); }
  return result;
}
struct Loaded { Json document; application::Spec spec; std::string creator; int revision; };
class Service {
  std::string database_;
  storage::ObjectStore& objects_;
  application::Registry registry_=application::builtins();
  static void scope(Database& db,const std::string& app,const std::string& space) {
    db.query("SELECT set_config('app.application_id',$1,true),set_config('app.space_id',$2,true)",{app,space});
  }
  static Loaded load(Database& db,const Principal& principal,const std::string& app,bool edit=false) {
    ensure(storage::valid_key(app),"not_found","Application not found.");
    const auto r=db.query("SELECT v.spec::text,a.creator_id,a.head_revision::text FROM applications a JOIN application_versions v ON v.workspace_id=a.workspace_id AND v.app_id=a.id AND v.revision=a.head_revision WHERE a.workspace_id=$1 AND a.id=$2 AND (a.creator_id=$3 OR EXISTS(SELECT 1 FROM application_memberships m WHERE m.workspace_id=a.workspace_id AND m.app_id=a.id AND m.principal_id=$3))"+std::string(edit?" FOR UPDATE OF a":" FOR SHARE OF a"),{principal.workspace,app,principal.id});
    ensure(r.size()==1,"not_found","Application not found.");auto doc=application::read_document(r.get(0,0));
    return {doc,application::spec_from_json(doc),r.get(0,1),std::stoi(r.get(0,2))};
  }
  static std::string member(Database& db,const Principal& p,const std::string& app,const std::string& space,bool lock=true) {
    ensure(storage::valid_key(space),"not_found","Data space not found.");
    auto r=db.query("SELECT m.role FROM application_spaces s JOIN application_memberships m ON m.workspace_id=s.workspace_id AND m.app_id=s.app_id AND m.space_id=s.id WHERE s.workspace_id=$1 AND s.app_id=$2 AND s.id=$3 AND m.principal_id=$4"+std::string(lock?" FOR UPDATE OF s":" FOR SHARE OF s"),{p.workspace,app,space,p.id});
    ensure(r.size()==1,"not_found","Data space not found.");scope(db,app,space);return r.get(0,0);
  }
  static void log(Database& db,const Principal& p,const std::string& app,const std::string& space,const std::string& kind,const Json& detail=Json::object()) {
    db.query("INSERT INTO application_events(workspace_id,app_id,space_id,principal_id,kind,detail) VALUES($1,$2,$3,$4,$5,$6::jsonb)",{p.workspace,app,space,p.id,kind,detail.dump()});
  }
  static Json spaces(Database& db,const Principal& p,const std::string& app) {
    auto r=db.query("SELECT s.id,s.name,m.role FROM application_spaces s JOIN application_memberships m ON m.workspace_id=s.workspace_id AND m.app_id=s.app_id AND m.space_id=s.id WHERE s.workspace_id=$1 AND s.app_id=$2 AND m.principal_id=$3 ORDER BY s.created_at,s.id",{p.workspace,app,p.id});
    auto out=Json::array();for(int i=0;i<r.size();++i)out.push_back({{"id",r.get(i,0)},{"name",r.get(i,1)},{"role",r.get(i,2)}});return out;
  }
  static std::string add_space(Database& db,const Principal& p,const std::string& app,const std::string& name) {
    ensure(!name.empty() && name.size()<=160,"invalid_space","A bounded data-space name is required.");
    const auto count=db.query("SELECT count(*)::text FROM application_spaces WHERE workspace_id=$1 AND app_id=$2",{p.workspace,app});ensure(std::stoull(count.get(0,0))<100,"space_limit","Local application space limit reached.");
    const auto id=random_id();
    db.query("INSERT INTO application_spaces(workspace_id,app_id,id,name) VALUES($1,$2,$3,$4)",{p.workspace,app,id,name});
    db.query("INSERT INTO application_memberships(workspace_id,app_id,space_id,principal_id,role) VALUES($1,$2,$3,$4,'owner')",{p.workspace,app,id,p.id});scope(db,app,id);log(db,p,app,id,"space.created");return id;
  }
  Json app_response(Database& db,const Principal& p,const std::string& id,const Loaded& app) const {
    return {{"id",id},{"revision",app.revision},{"spec",app.document},{"capabilities",application::capability_report(app.spec,registry_)},{"spaces",spaces(db,p,id)},{"can_edit_schema",app.creator==p.id && p.can_build}};
  }
  static Json row(Database& db,const Principal& p,const std::string& app,const std::string& space,const application::Entity& entity,const std::string& id) {
    ensure(storage::valid_key(id),"not_found","Record not found.");
    const auto r=db.query("SELECT data::text,row_version::text FROM application_records WHERE workspace_id=$1 AND app_id=$2 AND space_id=$3 AND entity_id=$4 AND id=$5",{p.workspace,app,space,entity.id,id});
    ensure(r.size()==1,"not_found","Record not found.");
    return {{"id",id},{"version",std::stoi(r.get(0,1))},{"values",application::record_to_json(application::normalize(entity.fields,application::record_from_json(application::read_document(r.get(0,0)))))}};
  }
  static void validate_links(Database& db,const Principal& p,const std::string& app,const std::string& space,const Loaded& spec,const application::Entity& entity,const application::Record& values,const std::string& role,const std::string& current={}) {
    for(const auto& field:entity.fields) {
      const auto& value=values.at(field.id);if(std::holds_alternative<std::monostate>(value))continue;
      if(field.unique) {
        const auto r=db.query("SELECT record_id FROM application_unique_values WHERE workspace_id=$1 AND app_id=$2 AND space_id=$3 AND entity_id=$4 AND field_id=$5 AND value=$6 AND record_id<>$7",{p.workspace,app,space,entity.id,field.id,std::get<std::string>(value),current});
        ensure(r.size()==0,"unique_conflict","A unique field already has this value: "+field.id);
      }
      if(field.kind==application::Kind::reference) {
        application::authorize(application::entity(spec.spec,field.target).read_roles,role);
        const auto r=db.query("SELECT 1 FROM application_records WHERE workspace_id=$1 AND app_id=$2 AND space_id=$3 AND entity_id=$4 AND id=$5",{p.workspace,app,space,field.target,std::get<std::string>(value)});
        ensure(r.size()==1,"invalid_reference","Referenced record is not available in this data space.");
      }
      if(field.kind==application::Kind::file) {
        application::authorize(spec.spec.storage.read_roles,role);
        const auto r=db.query("SELECT 1 FROM application_assets WHERE workspace_id=$1 AND app_id=$2 AND space_id=$3 AND id=$4 AND state='ready'",{p.workspace,app,space,std::get<std::string>(value)});
        ensure(r.size()==1,"invalid_reference","Referenced file is not ready in this data space.");
      }
    }
  }
  static void links(Database& db,const Principal& p,const std::string& app,const std::string& space,const application::Entity& entity,const std::string& id,const application::Record& values) {
    for(const auto* table:{"application_unique_values","application_edges","application_record_assets"})
      db.query(std::string("DELETE FROM ")+table+" WHERE workspace_id=$1 AND app_id=$2 AND space_id=$3 AND entity_id=$4 AND record_id=$5",{p.workspace,app,space,entity.id,id});
    for(const auto& f:entity.fields) {
      const auto& value=values.at(f.id);if(std::holds_alternative<std::monostate>(value))continue;
      if(f.unique) db.query("INSERT INTO application_unique_values(workspace_id,app_id,space_id,entity_id,field_id,value,record_id) VALUES($1,$2,$3,$4,$5,$6,$7)",{p.workspace,app,space,entity.id,f.id,std::get<std::string>(value),id});
      if(f.kind==application::Kind::reference)db.query("INSERT INTO application_edges(workspace_id,app_id,space_id,entity_id,record_id,field_id,target_entity,target_id) VALUES($1,$2,$3,$4,$5,$6,$7,$8)",{p.workspace,app,space,entity.id,id,f.id,f.target,std::get<std::string>(value)});
      if(f.kind==application::Kind::file)db.query("INSERT INTO application_record_assets(workspace_id,app_id,space_id,entity_id,record_id,field_id,asset_id) VALUES($1,$2,$3,$4,$5,$6,$7)",{p.workspace,app,space,entity.id,id,f.id,std::get<std::string>(value)});
    }
  }
  static Json save_row(Database& db,const Principal& p,const std::string& app,const std::string& space,const Loaded& loaded,const application::Entity& entity,const std::string& id,const application::Record& values,const std::string& role) {
    ensure(application::record_to_json(values).dump().size()<=256*1024,"record_limit","Record exceeds 256 KiB.");
    validate_links(db,p,app,space,loaded,entity,values,role,id);
    db.query("UPDATE application_records SET data=$6::jsonb,row_version=row_version+1,updated_at=now() WHERE workspace_id=$1 AND app_id=$2 AND space_id=$3 AND entity_id=$4 AND id=$5",{p.workspace,app,space,entity.id,id,application::record_to_json(values).dump()});
    links(db,p,app,space,entity,id,values);log(db,p,app,space,"record.updated",{{"entity",entity.id},{"record",id}});return row(db,p,app,space,entity,id);
  }
  Reply upload(const Principal& p,const Request& request,const std::string& app,const std::string& space) {
    const auto request_key=key(request), filename=request.parameter("filename","upload");
    ensure(!filename.empty() && filename.size()<=255 && filename.find_first_of("\r\n\0/\\",0,5)==std::string::npos,"invalid_filename","Use a simple file name with at most 255 bytes.");
    {Database gate(database_);gate.begin(p.workspace);const auto spec=load(gate,p,app);const auto role=member(gate,p,app,space,false);application::authorize(spec.spec.storage.write_roles,role);ensure(request.body.size()<=spec.spec.storage.max_file_bytes,"object_limit","File exceeds the configured application limit.");gate.commit();}
    const auto encoded=storage::encode(request.body);const auto input_hash=sha256(Json{{"filename",filename},{"sha256",encoded.sha256},{"size",encoded.original_size}}.dump());
    std::string id;
    {
      Database db(database_);db.begin(p.workspace);auto loaded=load(db,p,app);const auto role=member(db,p,app,space);application::authorize(loaded.spec.storage.write_roles,role);
      ensure(request.body.size()<=loaded.spec.storage.max_file_bytes,"object_limit","File exceeds this application's configured limit.");
      auto prior=db.query("SELECT id,input_hash FROM application_assets WHERE workspace_id=$1 AND app_id=$2 AND space_id=$3 AND request_key=$4",{p.workspace,app,space,request_key});
      if(prior.size()) {ensure(prior.get(0,1)==input_hash,"idempotency_conflict","Upload key was used for different content.");id=prior.get(0,0);}
      else {
        auto used=db.query("SELECT COALESCE(sum(original_size),0)::text,count(*)::text FROM application_assets WHERE workspace_id=$1 AND app_id=$2 AND space_id=$3",{p.workspace,app,space});
        ensure(std::stoull(used.get(0,0))+encoded.original_size<=loaded.spec.storage.max_space_bytes && std::stoull(used.get(0,1))<10000,"storage_quota","Storage quota exceeded, including pending uploads.");
        id=random_id();db.query("INSERT INTO application_assets(workspace_id,app_id,space_id,id,filename,media_type,original_size,stored_size,sha256,codec,state,request_key,input_hash) VALUES($1,$2,$3,$4,$5,'application/octet-stream',$6::bigint,$7::bigint,$8,$9,'pending',$10,$11)",{p.workspace,app,space,id,filename,std::to_string(encoded.original_size),std::to_string(encoded.stored_size),encoded.sha256,encoded.codec,request_key,input_hash});
        log(db,p,app,space,"asset.reserved",{{"asset",id}});
      }
      db.commit();
    }
    // Byte-store I/O deliberately happens outside PostgreSQL transactions.
    try {objects_.put(id,encoded.bytes);}catch(const application::Invalid& error){
      if(error.code!="object_exists")throw;
      ensure(storage::hex_hash(storage::checksum(storage::decode(objects_.get(id))))==encoded.sha256,"corrupt_object","A previously stored object does not match the upload.");
    }
    Database db(database_);db.begin(p.workspace);auto loaded=load(db,p,app);const auto role=member(db,p,app,space);application::authorize(loaded.spec.storage.write_roles,role);
    auto update=db.query("UPDATE application_assets SET state='ready' WHERE workspace_id=$1 AND app_id=$2 AND space_id=$3 AND id=$4 AND state='pending' RETURNING id",{p.workspace,app,space,id});
    if(update.size()){log(db,p,app,space,"asset.ready",{{"asset",id},{"sha256",encoded.sha256},{"codec",encoded.codec}});}
    db.commit();
    return {{{"id",id},{"filename",filename},{"codec",encoded.codec},{"sha256",encoded.sha256},{"original_size",encoded.original_size},{"stored_size",encoded.stored_size},{"state","ready"}},std::nullopt,201};
  }
public:
  Service(std::string database,storage::ObjectStore& store):database_(std::move(database)),objects_(store){}
  Reply handle(const Principal& p,const Request& request) {
    const auto route=parts(request.path);ensure(route.size()>=4 && route[0]=="api" && route[1]=="v1" && route[2]=="studio","not_found","Endpoint not found.");
    const bool get=request.method=="GET",post=request.method=="POST",patch=request.method=="PATCH";
    if(route.size()==9 && route[3]=="apps" && route[5]=="spaces" && route[7]=="assets" && route[8]=="upload" && post)return upload(p,request,route[4],route[6]);
    Json input;
    if(post||patch)input=application::read_document(request.body);
    if(route.size()==4 && route[3]=="compile" && post){ensure(p.can_build,"forbidden","Builder permission required.");const auto spec=application::spec_from_json(input);return {{{"valid",true},{"capabilities",application::capability_report(spec,registry_)},{"entities",spec.entities.size()},{"pages",spec.pages.size()}},std::nullopt,200};}
    if(route.size()==4 && route[3]=="meta" && get)return {{{"mode","development"},{"principal",p.id},{"can_build",p.can_build},{"core_capabilities",Json::array({"core.text.word_count@1"})},{"storage",{{"backend","local_object_store"},{"lossless_codec","zstd"},{"max_object_bytes",storage::max_object_bytes}}}},std::nullopt,200};
    Database db(database_);db.begin(p.workspace);Reply response;
    if(route.size()==4 && route[3]=="apps") {
      if(get) {
        auto result=db.query("SELECT a.id,a.name,a.head_revision::text FROM applications a WHERE a.workspace_id=$1 AND (a.creator_id=$2 OR EXISTS(SELECT 1 FROM application_memberships m WHERE m.workspace_id=a.workspace_id AND m.app_id=a.id AND m.principal_id=$2)) ORDER BY a.created_at DESC LIMIT 100",{p.workspace,p.id});
        response.document=Json::array();for(int i=0;i<result.size();++i)response.document.push_back({{"id",result.get(i,0)},{"name",result.get(i,1)},{"revision",std::stoi(result.get(i,2))}});
      } else if(post) {
        ensure(p.can_build,"forbidden","Builder permission required.");const auto spec=application::spec_from_json(input);const auto request_key=key(request), hash=sha256(input.dump());
        // Serialize application creation per authenticated workspace, not through user supplied IDs.
        db.query("SELECT pg_advisory_xact_lock(hashtextextended($1,0))",{p.workspace});
        auto prior=db.query("SELECT id,input_hash FROM applications WHERE workspace_id=$1 AND creator_id=$2 AND request_key=$3",{p.workspace,p.id,request_key});
        if(prior.size()){ensure(prior.get(0,1)==hash,"idempotency_conflict","Creation key was already used.");const auto id=prior.get(0,0);response.document=app_response(db,p,id,load(db,p,id));}
        else {
          auto count=db.query("SELECT count(*)::text FROM applications WHERE workspace_id=$1",{p.workspace});ensure(std::stoull(count.get(0,0))<100,"app_limit","Local workspace application limit reached.");
          const auto id=random_id();db.query("INSERT INTO applications(workspace_id,id,name,creator_id,request_key,input_hash) VALUES($1,$2,$3,$4,$5,$6)",{p.workspace,id,spec.name,p.id,request_key,hash});
          db.query("INSERT INTO application_versions(workspace_id,app_id,revision,spec) VALUES($1,$2,1,$3::jsonb)",{p.workspace,id,input.dump()});add_space(db,p,id,"Main workspace");response.document=app_response(db,p,id,{input,spec,p.id,1});response.status=201;
        }
      }else throw application::Invalid("not_found","Endpoint not found.");
    }else if(route.size()>=5 && route[3]=="apps") {
      const auto app=route[4];const bool revise=route.size()==6 && route[5]=="revisions" && post;
      const auto loaded=load(db,p,app,revise);
      if(route.size()==5 && get)response.document=app_response(db,p,app,loaded);
      else if(revise) {
        ensure(p.can_build && loaded.creator==p.id,"forbidden","Only the application builder can revise its schema.");application::keys(input,{"expected_revision","spec"},{"expected_revision","spec"});
        ensure(input.at("expected_revision")==loaded.revision,"revision_conflict","Application revision changed. Reload before publishing.");
        const auto next=application::spec_from_json(input.at("spec"));application::compatible_upgrade(loaded.spec,next);
        auto allspaces=db.query("SELECT id FROM application_spaces WHERE workspace_id=$1 AND app_id=$2 ORDER BY id FOR UPDATE",{p.workspace,app});
        for(int i=0;i<allspaces.size();++i){const auto space=allspaces.get(i,0);scope(db,app,space);
          for(const auto& e:next.entities){Json defaults=Json::object();for(const auto& f:e.fields)defaults[f.id]=application::value_to_json(f.default_value.value_or(application::Value{}));
            const auto maximum=db.query("SELECT COALESCE(max(octet_length(($5::jsonb || data)::text)),0)::text FROM application_records WHERE workspace_id=$1 AND app_id=$2 AND space_id=$3 AND entity_id=$4",{p.workspace,app,space,e.id,defaults.dump()});
            ensure(std::stoull(maximum.get(0,0))<=256*1024,"migration_required","A data backfill would exceed record size limits.");
            // Only add absent fields. Existing values and constraints are never overwritten.
            db.query("UPDATE application_records SET data=$5::jsonb || data,row_version=row_version+1,updated_at=now() WHERE workspace_id=$1 AND app_id=$2 AND space_id=$3 AND entity_id=$4 AND data <> ($5::jsonb || data)",{p.workspace,app,space,e.id,defaults.dump()});
          }log(db,p,app,space,"application.revised",{{"revision",loaded.revision+1}});
        }
        db.query("INSERT INTO application_versions(workspace_id,app_id,revision,spec) VALUES($1,$2,$3::integer,$4::jsonb)",{p.workspace,app,std::to_string(loaded.revision+1),input.at("spec").dump()});
        db.query("UPDATE applications SET name=$3,head_revision=head_revision+1 WHERE workspace_id=$1 AND id=$2",{p.workspace,app,next.name});
        response.document=app_response(db,p,app,{input.at("spec"),next,p.id,loaded.revision+1});
      }else if(route.size()==6 && route[5]=="spaces" && post){ensure(p.can_build && loaded.creator==p.id,"forbidden","Builder permission required.");application::keys(input,{"name"},{"name"});response.document={{"id",add_space(db,p,app,input.at("name").get<std::string>())}};response.status=201;}
      else if(route.size()>=7 && route[5]=="spaces") {
        const auto space=route[6];const auto role=member(db,p,app,space,!get);ensure(loaded.spec.roles.contains(role),"forbidden","Role is no longer declared.");
        if(route.size()==7 && get)response.document={{"id",space},{"role",role}};
        else if(route.size()==8 && route[7]=="members" && post){application::authorize({"owner"},role);application::keys(input,{"principal_id","role"},{"principal_id","role"});const auto who=input.at("principal_id").get<std::string>(),assigned=input.at("role").get<std::string>();
          ensure(who=="development-owner"||who=="development-reader","unknown_identity","This local build only has separately authenticated development identities.");ensure(loaded.spec.roles.contains(assigned),"unknown_role","Role not declared.");ensure(who!=p.id || assigned=="owner","forbidden","Cannot remove your own ownership.");
          db.query("INSERT INTO application_memberships(workspace_id,app_id,space_id,principal_id,role) VALUES($1,$2,$3,$4,$5) ON CONFLICT(workspace_id,app_id,space_id,principal_id) DO UPDATE SET role=EXCLUDED.role",{p.workspace,app,space,who,assigned});log(db,p,app,space,"membership.changed",{{"principal",who},{"role",assigned}});response.document={{"role",assigned}};
        }else if(route.size()==8 && route[7]=="events" && get){application::authorize({"owner"},role);auto events=db.query("SELECT sequence::text,kind,detail::text,created_at::text FROM application_events WHERE workspace_id=$1 AND app_id=$2 AND space_id=$3 ORDER BY sequence DESC LIMIT 100",{p.workspace,app,space});response.document=Json::array();for(int i=0;i<events.size();++i)response.document.push_back({{"sequence",events.get(i,0)},{"kind",events.get(i,1)},{"detail",application::read_document(events.get(i,2))},{"at",events.get(i,3)}});}
        else if(route.size()>=8 && route[7]=="assets" && get){application::authorize(loaded.spec.storage.read_roles,role);
          if(route.size()==8){auto files=db.query("SELECT id,filename,original_size::text,stored_size::text,codec,sha256,state FROM application_assets WHERE workspace_id=$1 AND app_id=$2 AND space_id=$3 ORDER BY created_at DESC LIMIT 200",{p.workspace,app,space});response.document=Json::array();for(int i=0;i<files.size();++i)response.document.push_back({{"id",files.get(i,0)},{"filename",files.get(i,1)},{"original_size",files.get(i,2)},{"stored_size",files.get(i,3)},{"codec",files.get(i,4)},{"sha256",files.get(i,5)},{"state",files.get(i,6)}});}
          else if(route.size()==10 && route[9]=="content") {const auto id=route[8];ensure(storage::valid_key(id),"not_found","Asset not found.");auto asset=db.query("SELECT sha256,original_size::text FROM application_assets WHERE workspace_id=$1 AND app_id=$2 AND space_id=$3 AND id=$4 AND state='ready'",{p.workspace,app,space,id});ensure(asset.size()==1,"not_found","Asset not found.");const auto hash=asset.get(0,0),size=asset.get(0,1);db.commit();auto bytes=storage::decode(objects_.get(id));ensure(sha256(bytes)==hash && std::to_string(bytes.size())==size,"corrupt_object","Object metadata does not match stored content.");return {Json::object(),std::move(bytes),200};}
          else throw application::Invalid("not_found","Endpoint not found.");
        }else if(route.size()>=9 && route[7]=="records"){
          const auto& entity=application::entity(loaded.spec,route[8]);application::authorize(get?entity.read_roles:entity.write_roles,role);
          if(route.size()==9 && get){const auto cursor=request.parameter("after");ensure(cursor.empty()||storage::valid_key(cursor),"invalid_cursor","Invalid record cursor.");
            auto records=db.query("SELECT id,data::text,row_version::text FROM application_records WHERE workspace_id=$1 AND app_id=$2 AND space_id=$3 AND entity_id=$4 AND id>$5 ORDER BY id LIMIT 101",{p.workspace,app,space,entity.id,cursor});auto items=Json::array();for(int i=0;i<std::min(records.size(),100);++i)items.push_back({{"id",records.get(i,0)},{"version",std::stoi(records.get(i,2))},{"values",application::record_to_json(application::normalize(entity.fields,application::record_from_json(application::read_document(records.get(i,1)))))}});response.document={{"items",items},{"next_cursor",records.size()>100?Json(records.get(99,0)):Json(nullptr)}};
          }else if(route.size()==9 && post){application::keys(input,{"values"},{"values"});const auto request_key=key(request),hash=sha256(input.dump());const auto values=application::normalize(entity.fields,application::record_from_json(input.at("values")));
            auto prior=db.query("SELECT id,input_hash FROM application_records WHERE workspace_id=$1 AND app_id=$2 AND space_id=$3 AND entity_id=$4 AND request_key=$5",{p.workspace,app,space,entity.id,request_key});
            if(prior.size()){ensure(prior.get(0,1)==hash,"idempotency_conflict","Record key was used for different content.");response.document=row(db,p,app,space,entity,prior.get(0,0));}
            else{ensure(application::record_to_json(values).dump().size()<=256*1024,"record_limit","Record exceeds 256 KiB.");const auto id=random_id();validate_links(db,p,app,space,loaded,entity,values,role);auto count=db.query("SELECT count(*)::text FROM application_records WHERE workspace_id=$1 AND app_id=$2 AND space_id=$3",{p.workspace,app,space});ensure(std::stoull(count.get(0,0))<10000,"record_limit","Data-space record limit reached.");
              db.query("INSERT INTO application_records(workspace_id,app_id,space_id,entity_id,id,data,request_key,input_hash) VALUES($1,$2,$3,$4,$5,$6::jsonb,$7,$8)",{p.workspace,app,space,entity.id,id,application::record_to_json(values).dump(),request_key,hash});links(db,p,app,space,entity,id,values);log(db,p,app,space,"record.created",{{"entity",entity.id},{"record",id}});response.document=row(db,p,app,space,entity,id);response.status=201;}
          }else if(route.size()==10 && get)response.document=row(db,p,app,space,entity,route[9]);
          else if(route.size()==10 && patch){application::keys(input,{"expected_version","values"},{"expected_version","values"});auto current=row(db,p,app,space,entity,route[9]);ensure(input.at("expected_version")==current.at("version"),"version_conflict","Record changed. Reload before saving.");auto merged=current.at("values");for(const auto& [k,v]:input.at("values").items())merged[k]=v;const auto values=application::normalize(entity.fields,application::record_from_json(merged));response.document=save_row(db,p,app,space,loaded,entity,route[9],values,role);}
          else if(route.size()==11 && route[10]=="delete" && post){application::keys(input,{"expected_version"},{"expected_version"});const auto id=route[9];auto current=row(db,p,app,space,entity,id);ensure(input.at("expected_version")==current.at("version"),"version_conflict","Record changed.");
            auto inbound=db.query("SELECT 1 FROM application_edges WHERE workspace_id=$1 AND app_id=$2 AND space_id=$3 AND target_entity=$4 AND target_id=$5 AND NOT(entity_id=$4 AND record_id=$5) LIMIT 1",{p.workspace,app,space,entity.id,id});ensure(!inbound.size(),"reference_conflict","Other records still reference this record.");
            for(const auto* table:{"application_unique_values","application_edges","application_record_assets"})db.query(std::string("DELETE FROM ")+table+" WHERE workspace_id=$1 AND app_id=$2 AND space_id=$3 AND entity_id=$4 AND record_id=$5",{p.workspace,app,space,entity.id,id});
            db.query("DELETE FROM application_records WHERE workspace_id=$1 AND app_id=$2 AND space_id=$3 AND entity_id=$4 AND id=$5",{p.workspace,app,space,entity.id,id});log(db,p,app,space,"record.deleted",{{"entity",entity.id},{"record",id}});response.document={{"deleted",id}};
          }else throw application::Invalid("not_found","Endpoint not found.");
        }else if(route.size()==10 && route[7]=="actions" && route[9]=="invoke" && post){application::keys(input,{"record_id","expected_version"},{"record_id","expected_version"});const auto& action=application::action(loaded.spec,route[8]);application::authorize(action.roles,role);const auto& entity=application::entity(loaded.spec,action.entity);application::authorize(entity.write_roles,role);const auto request_key=key(request),hash=sha256(Json{{"action",action.id},{"input",input}}.dump());
          auto prior=db.query("SELECT input_hash,response::text FROM application_invocations WHERE workspace_id=$1 AND app_id=$2 AND space_id=$3 AND request_key=$4",{p.workspace,app,space,request_key});
          if(prior.size()){ensure(prior.get(0,0)==hash,"idempotency_conflict","Action key was used for another invocation.");response.document=application::read_document(prior.get(0,1));}
          else{auto current=row(db,p,app,space,entity,input.at("record_id").get<std::string>());ensure(input.at("expected_version")==current.at("version"),"version_conflict","Record changed before invocation.");
            const auto values=application::invoke(loaded.spec,action,application::record_from_json(current.at("values")),role,registry_);response.document=save_row(db,p,app,space,loaded,entity,input.at("record_id").get<std::string>(),values,role);
            db.query("INSERT INTO application_invocations(workspace_id,app_id,space_id,request_key,input_hash,response) VALUES($1,$2,$3,$4,$5,$6::jsonb)",{p.workspace,app,space,request_key,hash,response.document.dump()});log(db,p,app,space,"capability.completed",{{"capability",action.capability},{"revision",loaded.revision},{"record",current.at("id")}});
          }
        }else throw application::Invalid("not_found","Endpoint not found.");
      }else throw application::Invalid("not_found","Endpoint not found.");
    }else throw application::Invalid("not_found","Endpoint not found.");
    db.commit();return response;
  }
};
}
