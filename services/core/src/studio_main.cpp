#include "runtime/studio.hpp"
#include "runtime/generation.hpp"
#include <fstream>
#include <future>
#include <drogon/drogon.h>
#include <iostream>

namespace {
std::string setting(const char* name,std::string fallback={}) {const auto* value=std::getenv(name);return value?value:fallback;}
int error_status(const std::string& code) {
  if(code=="unauthorized")return 401;
  if(code=="forbidden")return 403;
  if(code=="not_found")return 404;
  if(code.ends_with("conflict")||code=="migration_required")return 409;
  if(code=="capability_unavailable")return 424;
  if(code=="object_limit"||code=="document_limit")return 413;
  if(code=="planner_not_configured")return 503;
  if(code=="budget_limit")return 429;
  if(code=="storage_quota"||code=="record_limit"||code=="app_limit")return 422;
  if(code.starts_with("storage_")||code=="corrupt_object"||code=="compression_failed")return 503;
  return 400;
}
drogon::HttpResponsePtr response(const runtime::studio::Reply& result) {
  auto out=drogon::HttpResponse::newHttpResponse();out->setStatusCode(static_cast<drogon::HttpStatusCode>(result.status));
  if(result.bytes) {out->setContentTypeCode(drogon::CT_APPLICATION_OCTET_STREAM);out->setBody(*result.bytes);out->addHeader("Content-Disposition","attachment; filename=runtime-asset");}
  else {out->setContentTypeCode(drogon::CT_APPLICATION_JSON);out->setBody(result.document.dump());}
  out->addHeader("Cache-Control","no-store");out->addHeader("X-Content-Type-Options","nosniff");out->addHeader("X-Frame-Options","DENY");return out;
}
}
int main() {
  try {
    using runtime::application::ensure;using runtime::application::Json;
    ensure(setting("APP_ENV","development")=="development","configuration_error","Public production identity is not configured in this build.");
    const auto token=setting("DEV_API_TOKEN"),reader=setting("DEV_READER_TOKEN"),database=setting("DATABASE_URL"),origin=setting("APP_ORIGIN","http://localhost:8080");
    ensure(token.size()>=32 && !database.empty(),"configuration_error","Set a random DEV_API_TOKEN and DATABASE_URL.");
    ensure(reader.empty()||(reader.size()>=32 && reader!=token),"configuration_error","Reader token must be separate and random.");
    runtime::storage::LocalObjectStore objects(setting("OBJECT_ROOT","/var/lib/runtime/objects"));
    runtime::studio::Service studio(database,objects);
    auto number=[](const char* name,const char* fallback){return std::stoull(setting(name,fallback));};
    runtime::generation::Options planner;
    planner.enabled=setting("PLANNER_ENABLED")=="true";planner.model=setting("PLANNER_MODEL");
    planner.input_rate=number("PLANNER_INPUT_MICROUSD_PER_MILLION","0");planner.output_rate=number("PLANNER_OUTPUT_MICROUSD_PER_MILLION","0");
    planner.ceiling=number("GENERATION_CEILING_MICROUSD","250000");planner.monthly_limit=number("MODEL_MONTHLY_MICROUSD","100000000");
    ensure(planner.monthly_limit<=1000000000 && planner.ceiling<=1000000,"configuration_error","Model guards exceed this development build's bounds.");
    {std::ifstream file(setting("CONTRACTS_DIR","packages/contracts")+"/application.schema.json");ensure(file.good(),"configuration_error","Application contract is missing.");file>>planner.schema;}
    const auto bridge_token=setting("BRIDGE_TOKEN");
    if(planner.enabled)ensure(bridge_token.size()>=32,"configuration_error","Planner requires authenticated bridge access.");
    auto dispatch=[bridge_token](const Json& body)->Json{
      auto client=drogon::HttpClient::newHttpClient("http://bridge:8081",drogon::app().getLoop());
      auto request=drogon::HttpRequest::newHttpRequest();request->setMethod(drogon::Post);request->setPath("/internal/v1/compose");request->setContentTypeCode(drogon::CT_APPLICATION_JSON);request->setBody(body.dump());request->addHeader("Authorization","Bearer "+bridge_token);
      auto promise=std::make_shared<std::promise<std::pair<drogon::ReqResult,drogon::HttpResponsePtr>>>();auto future=promise->get_future();
      client->sendRequest(request,[promise,client](drogon::ReqResult result,const drogon::HttpResponsePtr& answer){(void)client;promise->set_value({result,answer});},95);
      ensure(future.wait_for(std::chrono::seconds(100))==std::future_status::ready,"provider_unknown","Planner transport outcome unknown.");
      auto [result,answer]=future.get();ensure(result==drogon::ReqResult::Ok && answer && answer->statusCode()==drogon::k200OK,"provider_unknown","Planner transport or provider outcome unknown.");
      return runtime::application::read_document(answer->getBody(),640*1024);
    };
    runtime::generation::Engine generator(database,std::move(planner),dispatch);runtime::Pool pool(4);
    auto handler=[&](const drogon::HttpRequestPtr& request,std::function<void(const drogon::HttpResponsePtr&)>&& callback){
      const auto cb=std::move(callback);
      if(!pool.submit([&,request,cb]{
        try {
          if(request->path()=="/health/live"){cb(response({{{"status","ok"},{"version","0.2.0"}},std::nullopt,200}));return;}
          if(request->path()=="/health/ready"){
            runtime::Database db(database);auto migrated=db.query("SELECT version FROM schema_migrations WHERE version=4");
            auto role=db.query("SELECT rolsuper OR rolbypassrls FROM pg_roles WHERE rolname=current_user");
            ensure(migrated.size()==1 && role.size()==1 && role.get(0,0)=="f","storage_unavailable","Database schema or application role is not ready.");
            cb(response({{{"status","ready"},{"version","0.2.0"}},std::nullopt,200}));return;
          }
          runtime::studio::Principal actor{"demo-workspace","",false};const auto auth=request->getHeader("authorization");
          if(runtime::secure_equal(auth,"Bearer "+token)){actor.id="development-owner";actor.can_build=true;}
          else if(!reader.empty()&&runtime::secure_equal(auth,"Bearer "+reader))actor.id="development-reader";
          else throw runtime::application::Invalid("unauthorized","A valid development access token is required.");
          ensure(request->getHeader("origin").empty()||request->getHeader("origin")==origin,"forbidden","Origin not allowed.");
          runtime::studio::Request input;input.method=request->methodString();input.path=request->path();input.body=std::string(request->getBody());input.idempotency_key=request->getHeader("idempotency-key");
          for(const auto& [key,value]:request->getParameters())input.query.emplace(key,value);
          if(input.path=="/api/v1/studio/generation" || input.path.starts_with("/api/v1/studio/generation/"))cb(response(generator.handle(actor,input)));
          else cb(response(studio.handle(actor,input)));
        }catch(const runtime::application::Invalid& error){cb(response({{{"error",{{"code",error.code},{"message",error.what()}}}},std::nullopt,error_status(error.code)}));}
        catch(const runtime::Error& error){std::cerr<<"runtime_error code="<<error.code<<'\n';cb(response({{{"error",{{"code",error.code},{"message",error.what()}}}},std::nullopt,503}));}
        catch(const nlohmann::json::exception&){cb(response({{{"error",{{"code","invalid_json"},{"message","JSON does not match the application contract."}}}},std::nullopt,400}));}
        catch(const std::exception&){std::cerr<<"runtime_error code=internal\n";cb(response({{{"error",{{"code","internal"},{"message","The operation failed without a confirmed result."}}}},std::nullopt,500}));}
      })){cb(response({{{"error",{{"code","busy"},{"message","Worker capacity reached. Retry later."}}}},std::nullopt,503}));}
    };
    drogon::app().registerHandlerViaRegex("^/(?:api/.*|health/.*)$",handler,{drogon::Get,drogon::Post,drogon::Patch});
    const auto web=setting("WEB_ROOT","apps/web/build");
    drogon::app().registerHandler("/",[web](const drogon::HttpRequestPtr&,std::function<void(const drogon::HttpResponsePtr&)>&& callback){auto out=drogon::HttpResponse::newFileResponse(web+"/200.html");out->addHeader("X-Frame-Options","DENY");out->addHeader("X-Content-Type-Options","nosniff");callback(out);},{drogon::Get});
    const int port=std::stoi(setting("PORT","8080"));ensure(port>0&&port<65536,"configuration_error","Invalid listen port.");
    drogon::app().setClientMaxBodySize(runtime::storage::max_object_bytes).setDocumentRoot(web).setThreadNum(2)
      .addListener(setting("BIND_ADDRESS","127.0.0.1"),static_cast<std::uint16_t>(port)).run();
  }catch(const std::exception& error){std::cerr<<"Runtime startup failed: "<<error.what()<<'\n';return 1;}
}
