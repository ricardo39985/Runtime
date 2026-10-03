#include "runtime/application.hpp"
#include "runtime/storage.hpp"
#include "runtime/platform.hpp"
#include <drogon/drogon.h>
#include <openssl/crypto.h>
#include <iostream>
#include <sstream>

namespace a = runtime::apps;
namespace j = boost::json;
namespace {
std::string env(const char* key,const std::string& fallback={}) {auto p=std::getenv(key); return p?std::string(p):fallback;}
struct Reply {int code; std::string body; std::string type="application/json"; std::string filename;};
Reply json(const j::value& v,int code=200) {return {code,j::serialize(v),"application/json",{}};}
bool opaque_id(std::string_view value) {return value.size()==32 && value.find_first_not_of("0123456789abcdef")==std::string_view::npos;}
std::string idem(const drogon::HttpRequestPtr& request) {
 auto value=request->getHeader("Idempotency-Key");
 a::need(value.size()>=16 && value.size()<=128 && value.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_")==value.npos,"invalid_key","Supply a unique Idempotency-Key of 16–128 characters."); return value;
}
std::vector<std::string> parts(const std::string& path) {
 std::vector<std::string> result; std::stringstream stream(path); std::string item;
 while(std::getline(stream,item,'/')) { if(!item.empty()) result.push_back(item); }
 return result;
}
void audit(runtime::Database& db,const std::string& ws,const std::string& app,const std::string& kind,const j::value& detail=j::object{}) {
 db.query("INSERT INTO application_events(workspace_id,app_id,kind,detail) VALUES($1,$2,$3,$4::jsonb)",{ws,app,kind,j::serialize(detail)});
}
struct Loaded {a::Application app; std::int64_t version;};
Loaded load(runtime::Database& db,const std::string& ws,const std::string& id,bool lock=false) {
 auto rows=db.query("SELECT spec::text,version::text FROM applications WHERE workspace_id=$1 AND id=$2"+std::string(lock?" FOR UPDATE":""),{ws,id});
 a::need(rows.size()==1,"not_found","Application not found."); return {a::Application(a::parse(rows.get(0,0))),std::stoll(rows.get(0,1))};
}
j::object record(runtime::Database& db,const std::string& ws,const std::string& app,const std::string& entity,const std::string& id) {
 auto rows=db.query("SELECT data::text,version::text FROM application_records WHERE workspace_id=$1 AND app_id=$2 AND entity=$3 AND id=$4 AND NOT archived",{ws,app,entity,id});
 a::need(rows.size()==1,"not_found","Record not found."); return {{"id",id},{"version",std::stoll(rows.get(0,1))},{"data",a::parse(rows.get(0,0))}};
}
void references(runtime::Database& db,const std::string& ws,const std::string& app,const a::Application& spec,const std::string& entity,const j::object& data) {
 for(const auto& value:spec.entity(entity).at("fields").as_array()) {
  const auto& field=value.as_object(); auto key=a::str(field,"key"),type=a::str(field,"type"); auto* supplied=data.if_contains(key);
  if(!supplied || supplied->is_null()) continue;
  if(type=="reference") {spec.authorize(a::str(field,"entity"),"owner",false); record(db,ws,app,a::str(field,"entity"),a::text(*supplied));}
  if(type=="file") a::need(db.query("SELECT id FROM application_files WHERE workspace_id=$1 AND app_id=$2 AND id=$3",{ws,app,a::text(*supplied)}).size()==1,"invalid_reference","File does not belong to this application.");
 }
}
void links(runtime::Database& db,const std::string& ws,const std::string& app,const a::Application& spec,const std::string& entity,const std::string& id,const j::object& data) {
 db.query("DELETE FROM application_record_links WHERE workspace_id=$1 AND app_id=$2 AND entity=$3 AND record_id=$4",{ws,app,entity,id});
 db.query("DELETE FROM application_file_links WHERE workspace_id=$1 AND app_id=$2 AND entity=$3 AND record_id=$4",{ws,app,entity,id});
 for(const auto& value:spec.entity(entity).at("fields").as_array()) {
  const auto& field=value.as_object(); auto key=a::str(field,"key"),type=a::str(field,"type"); auto* supplied=data.if_contains(key);
  if(!supplied || supplied->is_null()) continue;
  if(type=="reference") db.query("INSERT INTO application_record_links VALUES($1,$2,$3,$4,$5,$6,$7)",{ws,app,entity,id,key,a::str(field,"entity"),a::text(*supplied)});
  if(type=="file") db.query("INSERT INTO application_file_links VALUES($1,$2,$3,$4,$5,$6)",{ws,app,entity,id,key,a::text(*supplied)});
 }
}
j::object file_metadata(runtime::Database& db,const std::string& ws,const std::string& app,const std::string& id) {
 auto rows=db.query("SELECT f.filename,f.declared_mime,f.sha256,o.codec,o.original_size::text,o.stored_size::text FROM application_files f JOIN application_objects o ON(o.workspace_id=f.workspace_id AND o.app_id=f.app_id AND o.sha256=f.sha256) WHERE f.workspace_id=$1 AND f.app_id=$2 AND f.id=$3",{ws,app,id});
 a::need(rows.size()==1,"not_found","File not found.");
 return {{"id",id},{"filename",rows.get(0,0)},{"declared_mime",rows.get(0,1)},{"sha256",rows.get(0,2)},{"codec",rows.get(0,3)},{"original_size",std::stoll(rows.get(0,4))},{"stored_size",std::stoll(rows.get(0,5))}};
}
class Service {
 std::string database_,token_,origin_;
 const std::string ws_="demo-workspace"; // Replaced by verified session context at the identity gate; never request input.
 a::Registry registry_=a::builtins();
public:
 Service(std::string database,std::string token,std::string origin):database_(std::move(database)),token_(std::move(token)),origin_(std::move(origin)){}
 Reply handle(const drogon::HttpRequestPtr& request) {
  const auto path=request->path();
  if(path=="/health/live") return json(j::object{{"status","ok"},{"version","0.2.0"},{"engine","application"}});
  if(path!="/health/ready") {
   a::need(runtime::secure_equal(request->getHeader("authorization"),"Bearer "+token_),"unauthorized","Development access token required.");
   a::need(request->getHeader("origin").empty() || request->getHeader("origin")==origin_,"forbidden","Origin not allowed.");
  }
  runtime::Database db(database_);
  if(path=="/health/ready") {
   a::need(db.query("SELECT version FROM schema_migrations WHERE version=3").size()==1,"not_ready","Apply application-engine migrations.");
   auto role=db.query("SELECT rolsuper OR rolbypassrls FROM pg_roles WHERE rolname=current_user");
   a::need(role.size()==1 && role.get(0,0)=="f","not_ready","Use the restricted application database role.");
   return json(j::object{{"status","ready"},{"engine","application"},{"schema_version",3}});
  }
  const bool get=request->method()==drogon::Get,post=request->method()==drogon::Post,patch=request->method()==drogon::Patch;
  if(path=="/api/v1/meta") return json(j::object{{"engine","application"},{"mode","development"},{"identity","development-owner"},{"planner","not_connected"},{"storage","postgres-transactional-blob"},{"max_file_bytes",a::max_asset_bytes},{"external_actions",false}});
  db.begin(ws_);
  auto result=dispatch(db,request,get,post,patch);
  db.commit(); return result;
 }
 Reply dispatch(runtime::Database& db,const drogon::HttpRequestPtr& request,bool get,bool post,bool patch) {
  auto p=parts(request->path());
  a::need(p.size()>=3 && p[0]=="api" && p[1]=="v1" && p[2]=="apps","not_found","Endpoint not found.");
  if(p.size()==3 && get) {
   auto rows=db.query("SELECT id,name,app_key,version::text FROM applications WHERE workspace_id=$1 ORDER BY created_at DESC LIMIT 100",{ws_}); j::array values;
   for(int i=0;i<rows.size();++i) values.push_back(j::object{{"id",rows.get(i,0)},{"name",rows.get(i,1)},{"key",rows.get(i,2)},{"version",std::stoll(rows.get(i,3))}});
   return json(values);
  }
  if(p.size()==3 && post) {
   a::Application app(a::parse(request->getBody())); auto id=a::new_id();
   db.query("SELECT pg_advisory_xact_lock(hashtextextended($1,37))",{ws_});
   auto exists=db.query("SELECT id,spec::text FROM applications WHERE workspace_id=$1 AND app_key=$2 FOR UPDATE",{ws_,a::str(app.spec,"key")});
   if(exists.size()) {a::need(a::parse(exists.get(0,1))==j::value(app.spec),"conflict","An application with this key exists; use a versioned update."); return json(j::object{{"id",exists.get(0,0)},{"reused",true}});}
   auto quota=db.query("SELECT count(*)::text FROM applications WHERE workspace_id=$1",{ws_});
   a::need(std::stoll(quota.get(0,0))<100,"quota_exceeded","Workspace application limit reached.");
   db.query("INSERT INTO applications(workspace_id,id,app_key,name,version,spec) VALUES($1,$2,$3,$4,1,$5::jsonb)",{ws_,id,a::str(app.spec,"key"),a::str(app.spec,"name"),j::serialize(app.spec)});
   db.query("INSERT INTO application_versions(workspace_id,app_id,version,spec) VALUES($1,$2,1,$3::jsonb)",{ws_,id,j::serialize(app.spec)});
   audit(db,ws_,id,"application_created"); return json(j::object{{"id",id},{"version",1},{"ui",app.describe(registry_,"owner")}},201);
  }
  a::need(p.size()>=4 && opaque_id(p[3]),"not_found","Application not found."); const auto app_id=p[3];
  auto loaded=load(db,ws_,app_id,post||patch); auto& app=loaded.app;
  if(p.size()==4 && get) return json(j::object{{"id",app_id},{"version",loaded.version},{"spec",app.spec},{"ui",app.describe(registry_,"owner")}});
  if(p.size()==4 && patch) {
   auto input=a::parse(request->getBody()).as_object(); a::keys(input,{"version","spec"});
   a::need(a::integer(input.at("version"))==loaded.version,"conflict","Application changed; reload before updating.");
   a::Application next(input.at("spec")); next.compatible_with(app);
   db.query("UPDATE applications SET name=$3,spec=$4::jsonb,version=version+1,updated_at=now() WHERE workspace_id=$1 AND id=$2",{ws_,app_id,a::str(next.spec,"name"),j::serialize(next.spec)});
   db.query("INSERT INTO application_versions(workspace_id,app_id,version,spec) VALUES($1,$2,$3::integer,$4::jsonb)",{ws_,app_id,std::to_string(loaded.version+1),j::serialize(next.spec)});
   audit(db,ws_,app_id,"application_updated",j::object{{"version",loaded.version+1}}); return json(j::object{{"id",app_id},{"version",loaded.version+1},{"spec",next.spec},{"ui",next.describe(registry_,"owner")}});
  }
  if(p.size()==5 && p[4]=="events" && get) {
   auto after=request->getParameter("after"); if(after.empty()) after="0";
   a::need(after.size()<=16 && after.find_first_not_of("0123456789")==after.npos,"invalid_value","Invalid event cursor.");
   auto rows=db.query("SELECT sequence::text,kind,detail::text,created_at::text FROM application_events WHERE workspace_id=$1 AND app_id=$2 AND sequence>$3::bigint ORDER BY sequence LIMIT 200",{ws_,app_id,after}); j::array events;
   for(int i=0;i<rows.size();++i) events.push_back(j::object{{"sequence",rows.get(i,0)},{"kind",rows.get(i,1)},{"detail",a::parse(rows.get(i,2))},{"at",rows.get(i,3)}});
   return json(events);
  }
  if(p.size()>=5 && p[4]=="files") {
   if(p.size()==5 && post) {
    // CPU-only encoding is bounded. Blob+metadata publication is one PostgreSQL transaction.
    const auto key=idem(request),filename=a::safe_filename(drogon::utils::urlDecode(request->getHeader("X-Filename")));
    auto mime=request->getHeader("Content-Type"); if(mime.empty())mime="application/octet-stream";
    a::need(mime.size()<=120 && mime.find_first_of("\r\n")==mime.npos,"invalid_file","Invalid media type.");
    auto stored=a::encode(request->getBody());
    auto previous=db.query("SELECT id,sha256,filename,declared_mime FROM application_files WHERE workspace_id=$1 AND app_id=$2 AND idempotency_key=$3",{ws_,app_id,key});
    if(previous.size()) {a::need(previous.get(0,1)==stored.sha256 && previous.get(0,2)==filename && previous.get(0,3)==mime,"conflict","Upload key reused for different content."); return json(file_metadata(db,ws_,app_id,previous.get(0,0)));}
    auto usage=db.query("SELECT count(*)::text,COALESCE(sum(o.original_size),0)::text FROM application_files f JOIN application_objects o ON(o.workspace_id=f.workspace_id AND o.app_id=f.app_id AND o.sha256=f.sha256) WHERE f.workspace_id=$1 AND f.app_id=$2",{ws_,app_id});
    a::need(std::stoll(usage.get(0,0))<500 && std::stoull(usage.get(0,1))+stored.original_size<=128*1024*1024,"quota_exceeded","Application file quota exceeded.");
    db.query("INSERT INTO application_objects(workspace_id,app_id,sha256,codec,original_size,stored_size,content) VALUES($1,$2,$3,$4,$5::bigint,$6::bigint,decode($7,'hex')) ON CONFLICT(workspace_id,app_id,sha256) DO NOTHING",{ws_,app_id,stored.sha256,stored.codec,std::to_string(stored.original_size),std::to_string(stored.bytes.size()),a::hex(stored.bytes)});
    auto id=a::new_id(); db.query("INSERT INTO application_files(workspace_id,app_id,id,sha256,filename,declared_mime,idempotency_key) VALUES($1,$2,$3,$4,$5,$6,$7)",{ws_,app_id,id,stored.sha256,filename,mime,key});
    audit(db,ws_,app_id,"file_stored",j::object{{"id",id},{"sha256",stored.sha256},{"codec",stored.codec},{"original_size",stored.original_size},{"stored_size",stored.bytes.size()}});
    return json(file_metadata(db,ws_,app_id,id),201);
   }
   if(p.size()==5 && get) {
    auto rows=db.query("SELECT id FROM application_files WHERE workspace_id=$1 AND app_id=$2 ORDER BY created_at DESC LIMIT 500",{ws_,app_id}); j::array files;
    for(int i=0;i<rows.size();++i) { files.push_back(file_metadata(db,ws_,app_id,rows.get(i,0))); }
    return json(files);
   }
   if(p.size()==7 && opaque_id(p[5]) && p[6]=="content" && get) {
    auto metadata=file_metadata(db,ws_,app_id,p[5]);
    auto content=db.query("SELECT encode(content,'hex') FROM application_objects WHERE workspace_id=$1 AND app_id=$2 AND sha256=$3",{ws_,app_id,a::str(metadata,"sha256")});
    a::StoredObject stored{a::str(metadata,"sha256"),a::str(metadata,"codec"),a::unhex(content.get(0,0)),static_cast<std::size_t>(a::integer(metadata.at("original_size")))};
    return {200,a::decode(stored),"application/octet-stream",a::str(metadata,"filename")};
   }
  }
  if(p.size()>=6 && p[4]=="records" && a::identifier(p[5])) {
   const auto entity=p[5]; app.authorize(entity,"owner",post||patch);
   if(p.size()==6 && get) {
    auto cursor=request->getParameter("after"); a::need(cursor.empty() || opaque_id(cursor),"invalid_value","Invalid record cursor.");
    auto rows=db.query("SELECT id,data::text,version::text FROM application_records WHERE workspace_id=$1 AND app_id=$2 AND entity=$3 AND NOT archived AND id>$4 ORDER BY id LIMIT 101",{ws_,app_id,entity,cursor}); j::array records;
    for(int i=0;i<std::min(rows.size(),100);++i) records.push_back(j::object{{"id",rows.get(i,0)},{"version",std::stoll(rows.get(i,2))},{"data",a::parse(rows.get(i,1))}});
    return json(j::object{{"items",records},{"next_cursor",rows.size()>100?j::value(rows.get(99,0)):j::value(nullptr)}});
   }
   if(p.size()==6 && post) {
    const auto key=idem(request); auto input=a::parse(request->getBody()).as_object(); a::keys(input,{"app_version","data"});
    a::need(a::integer(input.at("app_version"))==loaded.version,"conflict","Application schema changed; reload the form.");
    auto data=app.validate_record(entity,input.at("data")); auto fingerprint=a::hash(j::serialize(j::object{{"entity",entity},{"data",data}}));
    auto old=db.query("SELECT id,entity,request_hash FROM application_records WHERE workspace_id=$1 AND app_id=$2 AND idempotency_key=$3",{ws_,app_id,key});
    if(old.size()) {a::need(old.get(0,1)==entity && old.get(0,2)==fingerprint,"conflict","Request key already used with different data."); return json(record(db,ws_,app_id,entity,old.get(0,0)));}
    auto count=db.query("SELECT count(*)::text FROM application_records WHERE workspace_id=$1 AND app_id=$2",{ws_,app_id});
    a::need(std::stoll(count.get(0,0))<10000,"quota_exceeded","Application record quota reached."); references(db,ws_,app_id,app,entity,data);
    auto id=a::new_id(); db.query("INSERT INTO application_records(workspace_id,app_id,entity,id,data,idempotency_key,request_hash) VALUES($1,$2,$3,$4,$5::jsonb,$6,$7)",{ws_,app_id,entity,id,j::serialize(data),key,fingerprint});
    links(db,ws_,app_id,app,entity,id,data); audit(db,ws_,app_id,"record_created",j::object{{"entity",entity},{"id",id}}); return json(record(db,ws_,app_id,entity,id),201);
   }
   if(p.size()>=7 && opaque_id(p[6])) {
    const auto id=p[6]; auto existing=record(db,ws_,app_id,entity,id);
    if(p.size()==7 && get) return json(existing);
    if(p.size()==7 && patch) {
     auto input=a::parse(request->getBody()).as_object(); a::keys(input,{"app_version","version","data"});
     a::need(a::integer(input.at("app_version"))==loaded.version && a::integer(input.at("version"))==a::integer(existing.at("version")),"conflict","Record or application changed; reload before saving.");
     auto data=app.validate_record(entity,input.at("data")); references(db,ws_,app_id,app,entity,data);
     db.query("UPDATE application_records SET data=$5::jsonb,version=version+1,updated_at=now() WHERE workspace_id=$1 AND app_id=$2 AND entity=$3 AND id=$4",{ws_,app_id,entity,id,j::serialize(data)});
     links(db,ws_,app_id,app,entity,id,data); audit(db,ws_,app_id,"record_updated",j::object{{"entity",entity},{"id",id}}); return json(record(db,ws_,app_id,entity,id));
    }
    if(p.size()==8 && p[7]=="archive" && post) {
     auto input=a::parse(request->getBody()).as_object(); a::keys(input,{"version"});
     a::need(a::integer(input.at("version"))==a::integer(existing.at("version")),"conflict","Record changed; reload before archiving.");
     a::need(!db.query("SELECT record_id FROM application_record_links WHERE workspace_id=$1 AND app_id=$2 AND target_entity=$3 AND target_id=$4 LIMIT 1",{ws_,app_id,entity,id}).size(),"conflict","Another record refers to this record; remove the relationship first.");
     links(db,ws_,app_id,app,entity,id,j::object{});
     db.query("UPDATE application_records SET archived=true,version=version+1 WHERE workspace_id=$1 AND app_id=$2 AND entity=$3 AND id=$4",{ws_,app_id,entity,id});
     audit(db,ws_,app_id,"record_archived",j::object{{"entity",entity},{"id",id}}); return json(j::object{{"archived",true}});
    }
   }
  }
  if(p.size()==6 && p[4]=="actions" && a::identifier(p[5]) && post) {
   const auto& action=app.action(p[5]); const auto entity=a::str(action,"entity"); app.authorize(entity,"owner",true);
   auto input=a::parse(request->getBody()).as_object(); a::keys(input,{"app_version","record_id","record_version"});
   a::need(a::integer(input.at("app_version"))==loaded.version,"conflict","Application changed.");
   auto data=record(db,ws_,app_id,entity,a::str(input,"record_id"));
   a::need(a::integer(input.at("record_version"))==a::integer(data.at("version")),"conflict","Record changed.");
   auto resolution=registry_.resolve(action);
   if(!resolution.at("available").as_bool()) {
    audit(db,ws_,app_id,"capability_blocked",resolution);
    return json(j::object{{"error",j::object{{"code","capability_unavailable"},{"message","This action needs a compatible capability. Application data and files remain available."},{"detail",resolution}}}},424);
   }
   // Only trusted local read-only implementations are registered. No provider I/O is allowed here.
   auto output=registry_.invoke(action,data); audit(db,ws_,app_id,"capability_completed",j::object{{"action",p[5]},{"record_id",input.at("record_id")}}); return json(j::object{{"result",output}});
  }
  throw a::Fault("not_found","Endpoint not found.");
 }
};
int status(const std::string& code) {
 if(code=="unauthorized") return 401;
 if(code=="forbidden") return 403;
 if(code=="not_found") return 404;
 if(code=="conflict" || code=="migration_required") return 409;
 if(code=="input_limit") return 413;
 if(code=="quota_exceeded") return 429;
 if(code=="capability_unavailable") return 424;
 if(code=="not_ready" || code.starts_with("database")) return 503;
 if(code=="corrupt_object" || code=="storage_error") return 500;
 return 400;
}
}
int main() {
 try {
  a::need(env("APP_ENV","development")=="development" && env("PROVIDER_MODE","fixture")=="fixture","configuration_error","Public identity and external execution gates are not complete. Only local development startup is allowed.");
  const auto token=env("DEV_API_TOKEN"); a::need(token.size()>=32 && !env("DATABASE_URL").empty(),"configuration_error","Provide a random development token and a restricted DATABASE_URL.");
  Service service(env("DATABASE_URL"),token,env("APP_ORIGIN","http://localhost:8080")); runtime::Pool pool;
  auto handler=[&](const drogon::HttpRequestPtr& req,std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
   auto cb=std::move(callback);
   auto send=[cb](const Reply& reply) {
    auto response=drogon::HttpResponse::newHttpResponse(); response->setStatusCode(static_cast<drogon::HttpStatusCode>(reply.code)); response->setContentTypeString(reply.type); response->setBody(reply.body);
    response->addHeader("Cache-Control","no-store"); response->addHeader("X-Content-Type-Options","nosniff"); response->addHeader("X-Frame-Options","DENY");
    if(!reply.filename.empty()) {response->addHeader("Content-Disposition","attachment; filename=\"download.bin\""); response->addHeader("Content-Security-Policy","sandbox; default-src 'none'");}
    cb(response);
   };
   if(!pool.submit([&,req,send]{try{send(service.handle(req));}
    catch(const a::Fault& e){send(json(j::object{{"error",j::object{{"code",e.code},{"message",e.what()}}}},status(e.code)));}
    catch(const runtime::Error& e){send(json(j::object{{"error",j::object{{"code",e.code},{"message",e.what()}}}},status(e.code)));}
    catch(const std::exception&){send(json(j::object{{"error",j::object{{"code","invalid_request"},{"message","Invalid application request."}}}},400));}
   })) send(json(j::object{{"error",j::object{{"code","busy"},{"message","Server is busy; retry later."}}}},503));
  };
  drogon::app().registerHandlerViaRegex("^/(api/.*|health/.*)$",handler,{drogon::Get,drogon::Post,drogon::Patch});
  const auto web=env("WEB_ROOT","apps/web/build");
  drogon::app().registerHandler("/",[web](const drogon::HttpRequestPtr&,std::function<void(const drogon::HttpResponsePtr&)>&& cb){auto response=drogon::HttpResponse::newFileResponse(web+"/200.html"); response->addHeader("X-Frame-Options","DENY"); cb(response);},{drogon::Get});
  drogon::app().setClientMaxBodySize(a::max_asset_bytes).setClientMaxMemoryBodySize(a::max_asset_bytes).setDocumentRoot(web).setThreadNum(2).addListener(env("BIND_ADDRESS","127.0.0.1"),static_cast<std::uint16_t>(std::stoi(env("PORT","8080")))).run();
 } catch(const std::exception& e) {std::cerr<<"Application engine startup failed: "<<e.what()<<'\n'; return 1;}
}
