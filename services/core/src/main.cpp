#include "runtime/platform.hpp"
#include <drogon/drogon.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json-schema.hpp>
#include <atomic>
#include <fstream>
#include <iostream>
#include <cstdio>

using nlohmann::json;
using namespace runtime;
namespace {
std::string env(const char* name, std::string fallback = {}) { const char* v = std::getenv(name); return v ? v : fallback; }
std::int64_t now_seconds() { return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count(); }
std::string iso_date(std::chrono::sys_days value) {
  auto ymd = std::chrono::year_month_day{value}; char buffer[16];
  std::snprintf(buffer,sizeof buffer,"%04d-%02u-%02u",int(ymd.year()),unsigned(ymd.month()),unsigned(ymd.day())); return buffer;
}
json read_json(std::string_view text) {
  return json::parse(text,[](int depth,json::parse_event_t,json&) { require(depth < 32,"invalid_json","JSON nesting limit exceeded."); return true; });
}
json load_schema(const std::string& path) { std::ifstream file(path); require(file.good(),"configuration_error","Contract file missing."); json result; file >> result; return result; }
using Validator = nlohmann::json_schema::json_validator;
std::shared_ptr<const Validator> compile_schema(const std::string& path) {
  auto validator = std::make_shared<Validator>(); validator->set_root_schema(load_schema(path)); return validator;
}
void event(Database& db,const std::string& tenant,const std::string& id,const std::string& kind,const json& detail = json::object()) {
  db.query("INSERT INTO run_events(workspace_id,run_id,kind,detail) VALUES($1,$2,$3,$4::jsonb)",{tenant,id,kind,detail.dump()});
}
json load_run(Database& db,const std::string& tenant,const std::string& id,bool lock = false) {
  auto result = db.query("SELECT document::text,state,version::text,proposal_hash,COALESCE(EXTRACT(EPOCH FROM expires_at)::bigint,0)::text FROM runs WHERE workspace_id=$1 AND id=$2" + std::string(lock ? " FOR UPDATE" : ""),{tenant,id});
  require(result.size() == 1,"not_found","Run not found.");
  auto doc = json::parse(result.get(0,0)); doc["id"] = id; doc["status"] = result.get(0,1);
  doc["version"] = std::stoull(result.get(0,2)); doc["proposal_hash"] = result.get(0,3);
  doc["expires_at"] = std::stoll(result.get(0,4)); doc["mode"] = "fixture"; return doc;
}
std::string digest(const std::string& tenant,const std::string& id,const json& doc) {
  return sha256(json{{"workspace",tenant},{"run",id},{"tool","fixture.email.simulate@1"},{"drafts",doc.at("drafts")},{"source",doc.at("source")}}.dump());
}
json prepare(const json& request,const Validator& ui_schema) {
  const auto today = std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now());
  auto selection = select_overdue(import_invoices(request.at("csv").get<std::string>()),today,request.at("min_days").get<int>());
  json invoices = json::array(), drafts = json::array(), bindings = json::object(), components = json::array();
  for (const auto& invoice : selection.eligible) {
    invoices.push_back({{"id",invoice.id},{"customer",invoice.customer},{"email",invoice.email},{"amount_minor",std::to_string(invoice.amount)},
                        {"currency",invoice.currency},{"due_date",invoice.due_date}});
    if (drafts.size() < 10) drafts.push_back({{"invoice_id",invoice.id},{"to",invoice.email},{"subject","Follow-up on invoice " + invoice.id},
      {"body","Hello " + invoice.customer + ",\n\nOur imported snapshot lists invoice " + invoice.id + " as outstanding. Please let us know its current payment status.\n\nThank you."}});
  }
  for (const auto& [currency,total] : selection.totals) {
    const std::string key = "total_" + currency;
    bindings[key] = {{"amount_minor",std::to_string(total)},{"currency",currency}};
    components.push_back({{"type","metric"},{"label","Outstanding · " + currency},{"binding",key}});
  }
  bindings["invoices"] = invoices; components.push_back({{"type","table"},{"binding","invoices"}});
  json ui{{"schema_version",1},{"title",request.at("title")},{"components",components}}; ui_schema.validate(ui);
  return {{"title",request.at("title")},{"ui",ui},{"bindings",bindings},{"drafts",drafts},{"excluded",selection.excluded},
          {"not_in_batch",selection.eligible.size() - drafts.size()},{"source",{{"kind","csv_snapshot"},{"captured_at",now_seconds()},{"as_of",iso_date(today)}}}};
}
void process_job(const std::string& database,const std::string& tenant,const Validator& ui_schema) {
  Database db(database); db.begin(tenant);
  auto job = db.query("SELECT run_id,kind FROM jobs WHERE workspace_id=$1 ORDER BY created_at LIMIT 1 FOR UPDATE SKIP LOCKED",{tenant});
  if (!job.size()) { db.commit(); return; }
  const auto id = job.get(0,0), kind = job.get(0,1);
  auto row = db.query("SELECT request::text,state,document::text FROM runs WHERE workspace_id=$1 AND id=$2 FOR UPDATE",{tenant,id});
  if (row.size() && kind == "prepare" && row.get(0,1) == "queued") {
    try {
      const auto doc = prepare(json::parse(row.get(0,0)),ui_schema);
      const auto status = doc.at("drafts").empty() ? "completed" : "awaiting_approval";
      db.query("UPDATE runs SET document=$3::jsonb,state=$4,proposal_hash=$5,expires_at=now()+interval '30 minutes' WHERE workspace_id=$1 AND id=$2",{tenant,id,doc.dump(),status,digest(tenant,id,doc)});
      event(db,tenant,id,"prepared",{{"actions",doc.at("drafts").size()},{"mode","fixture"}});
    } catch (const std::exception&) {
      db.query("UPDATE runs SET state='failed',document=$3::jsonb WHERE workspace_id=$1 AND id=$2",{tenant,id,json{{"error","Workflow preparation failed."}}.dump()});
      event(db,tenant,id,"failed");
    }
  } else if (row.size() && kind == "simulate" && row.get(0,1) == "executing") {
    auto doc = json::parse(row.get(0,2));
    doc["receipts"] = json::array();
    for (const auto& draft : doc.at("drafts")) doc["receipts"].push_back({{"invoice_id",draft.at("invoice_id")},{"status","simulated"},{"external_action",false}});
    db.query("UPDATE runs SET document=$3::jsonb,state='completed' WHERE workspace_id=$1 AND id=$2",{tenant,id,doc.dump()});
    event(db,tenant,id,"simulation_completed",{{"external_actions",0}});
  }
  db.query("DELETE FROM jobs WHERE workspace_id=$1 AND run_id=$2 AND kind=$3",{tenant,id,kind}); db.commit();
}
struct App {
  std::string database,token,origin,tenant = "demo-workspace";
  std::shared_ptr<const Validator> request_schema,ui_schema;
  json request(Database& db,const json& input,const std::string& key) {
    try { request_schema->validate(input); } catch (const std::exception&) { throw Error("invalid_request","Request does not match the versioned invoice workflow contract."); }
    require(key.size() >= 16 && key.size() <= 128 && key.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_") == std::string::npos,"invalid_key","Supply a 16–128 character Idempotency-Key.");
    import_invoices(input.at("csv").get<std::string>());
    const auto hash = sha256(input.dump()); const auto id = random_id();
    auto inserted = db.query("INSERT INTO runs(workspace_id,id,idempotency_key,input_hash,request) VALUES($1,$2,$3,$4,$5::jsonb) ON CONFLICT(workspace_id,idempotency_key) DO NOTHING RETURNING id",{tenant,id,key,hash,input.dump()});
    if (!inserted.size()) {
      const auto existing = db.query("SELECT id,input_hash FROM runs WHERE workspace_id=$1 AND idempotency_key=$2",{tenant,key});
      require(existing.size() == 1 && existing.get(0,1) == hash,"idempotency_conflict","This key was already used for a different request."); return load_run(db,tenant,existing.get(0,0));
    }
    db.query("INSERT INTO jobs(workspace_id,run_id,kind) VALUES($1,$2,'prepare')",{tenant,id}); event(db,tenant,id,"queued"); return load_run(db,tenant,id);
  }
  json handle(const drogon::HttpRequestPtr& req,int& status) {
    const auto path = req->path();
    if (path == "/health/live") return {{"status","ok"},{"version","0.1.0"},{"mode","fixture"}};
    if (path != "/health/ready") {
      require(secure_equal(req->getHeader("authorization"),"Bearer " + token),"unauthorized","Development access token required.");
      require(req->getHeader("origin").empty() || req->getHeader("origin") == origin,"forbidden","Origin is not allowed.");
    }
    Database db(database);
    if (path == "/health/ready") {
      auto result = db.query("SELECT version FROM schema_migrations WHERE version=1");
      require(result.size() == 1,"database_unavailable","Database migration required.");
      auto role = db.query("SELECT rolsuper OR rolbypassrls FROM pg_roles WHERE rolname=current_user");
      require(role.size() == 1 && role.get(0,0) == "f","configuration_error","Core must use a restricted application database role.");
      return {{"status","ready"},{"mode","fixture"}};
    }
    db.begin(tenant);
    const bool post = req->method() == drogon::Post, patch = req->method() == drogon::Patch, get = req->method() == drogon::Get;
    json input;
    if (post || patch) { try { input = read_json(req->getBody()); } catch (const std::exception&) { throw Error("invalid_json","A bounded JSON request body is required."); } }
    json output;
    if (path == "/api/v1/meta" && get) output = {{"mode","fixture"},{"workspace","Development workspace"},{"planner","not_connected"},{"external_actions_enabled",false}};
    else if (path == "/api/v1/sample" && get) {
      auto today = std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now());
      output = {{"csv","invoice_id,customer,email,amount_minor,currency,due_date,status\nINV-1042,Northstar Studio,accounts@northstar.test,248000,USD," + iso_date(today-std::chrono::days(35)) + ",open\nINV-1048,Juniper Design,hello@juniper.test,95000,USD," + iso_date(today-std::chrono::days(23)) + ",open\nINV-1051,Cedar Works,billing@cedar.test,72000,USD," + iso_date(today-std::chrono::days(18)) + ",paid\n"}};
    } else if (path == "/api/v1/requests" && post) { output = request(db,input,req->getHeader("idempotency-key")); status = 202; }
    else if (path == "/api/v1/runs" && get) {
      auto rows = db.query("SELECT id,state,COALESCE(request->>'title','Untitled') FROM runs WHERE workspace_id=$1 ORDER BY created_at DESC LIMIT 30",{tenant});
      output = json::array(); for (int i=0;i<rows.size();++i) output.push_back({{"id",rows.get(i,0)},{"status",rows.get(i,1)},{"title",rows.get(i,2)}});
    } else if (path.starts_with("/api/v1/runs/")) {
      auto tail = path.substr(13); const auto slash = tail.find('/'); const auto id = tail.substr(0,slash);
      const auto action = slash == std::string::npos ? "" : tail.substr(slash + 1);
      require(id.size() == 32 && id.find_first_not_of("0123456789abcdef") == std::string::npos,"not_found","Run not found.");
      if (action.empty() && get) output = load_run(db,tenant,id);
      else if (action == "events" && get) {
        const auto after = req->getParameter("after"); const auto sequence = after.empty() ? 0 : minor_units(after);
        auto rows = db.query("SELECT sequence::text,kind,detail::text,created_at::text FROM run_events WHERE workspace_id=$1 AND run_id=$2 AND sequence>$3::bigint ORDER BY sequence LIMIT 200",{tenant,id,std::to_string(sequence)});
        load_run(db,tenant,id); output = json::array();
        for(int i=0;i<rows.size();++i) output.push_back({{"sequence",rows.get(i,0)},{"kind",rows.get(i,1)},{"detail",json::parse(rows.get(i,2))},{"at",rows.get(i,3)}});
      } else if (action == "cancel" && post) {
        auto doc = load_run(db,tenant,id,true); const auto state = doc.at("status").get<std::string>();
        require(state == "queued" || state == "awaiting_approval" || state == "cancelled","invalid_state","Run can no longer be cancelled.");
        if (state != "cancelled") { db.query("UPDATE runs SET state='cancelled' WHERE workspace_id=$1 AND id=$2",{tenant,id}); /* The worker removes cancelled jobs, preserving job-before-run lock order. */ event(db,tenant,id,"cancelled"); }
        output = load_run(db,tenant,id);
      } else if (action == "drafts" && patch) {
        auto doc = load_run(db,tenant,id,true);
        require(doc.at("status") == "awaiting_approval","invalid_state","Only pending drafts can be edited.");
        require(input.is_object() && input.size() == 3 && input.contains("version") && input.contains("index") && input.contains("body"),"invalid_request","Use version, index, and body.");
        require(input.at("version") == doc.at("version"),"stale_approval","Draft version changed.");
        require(input.at("index").is_number_unsigned() && input.at("body").is_string(),"invalid_request","Invalid draft edit.");
        auto index = input.at("index").get<std::size_t>(); auto body = input.at("body").get<std::string>();
        require(index < doc.at("drafts").size() && !body.empty() && body.size() <= 10000,"invalid_request","Invalid draft edit.");
        doc["drafts"][index]["body"] = body;
        db.query("UPDATE runs SET document=$3::jsonb,version=version+1,proposal_hash=$4,expires_at=now()+interval '30 minutes' WHERE workspace_id=$1 AND id=$2",{tenant,id,doc.dump(),digest(tenant,id,doc)});
        event(db,tenant,id,"draft_edited"); output = load_run(db,tenant,id);
      } else if (action == "approve" && post) {
        auto doc = load_run(db,tenant,id,true);
        require(input.is_object() && input.size() == 3 && input.contains("version") && input.contains("proposal_hash") && input.contains("snapshot_ack"),"invalid_request","Review the exact proposal and acknowledge the CSV snapshot.");
        require(input.at("snapshot_ack") == true && input.at("version").is_number_unsigned() && input.at("proposal_hash").is_string(),"invalid_request","Snapshot acknowledgement and exact version/hash required.");
        const auto version = input.at("version").get<std::uint64_t>(); const auto hash = input.at("proposal_hash").get<std::string>();
        require(doc.at("version") == version && doc.at("proposal_hash") == hash,"stale_approval","Proposal changed. Review it again.");
        if (doc.at("status") == "executing" || doc.at("status") == "completed") output = doc;
        else {
          require(doc.at("status") == "awaiting_approval","invalid_state","Run is not awaiting approval.");
          validate_approval({tenant,doc.at("proposal_hash"),doc.at("version"),doc.at("expires_at")},tenant,"owner",hash,version,now_seconds(),doc.at("drafts").size());
          require(digest(tenant,id,doc) == hash,"stale_approval","Payload changed.");
          db.query("UPDATE runs SET state='executing' WHERE workspace_id=$1 AND id=$2",{tenant,id});
          db.query("INSERT INTO jobs(workspace_id,run_id,kind) VALUES($1,$2,'simulate') ON CONFLICT DO NOTHING",{tenant,id});
          event(db,tenant,id,"approved",{{"proposal_hash",hash},{"version",version},{"principal","development-owner"}}); output = load_run(db,tenant,id); status = 202;
        }
      } else throw Error("not_found","Endpoint not found.");
    } else throw Error("not_found","Endpoint not found.");
    db.commit(); return output;
  }
};
}
int main() {
  try {
    require(env("APP_ENV","development") == "development" && env("PROVIDER_MODE","fixture") == "fixture",
            "configuration_error","This milestone supports local development only. Live/production startup is disabled.");
    App service{env("DATABASE_URL"),env("DEV_API_TOKEN"),env("APP_ORIGIN","http://localhost:8080"),"demo-workspace",
                compile_schema(env("CONTRACTS_DIR","packages/contracts")+"/request.schema.json"),compile_schema(env("CONTRACTS_DIR","packages/contracts")+"/ui.schema.json")};
    require(service.token.size() >= 32 && service.token != "replace-with-a-random-development-token","configuration_error","Set a random DEV_API_TOKEN of at least 32 characters.");
    require(!service.database.empty(),"configuration_error","DATABASE_URL is required.");
    std::atomic<bool> polling = false; Pool pool;
    auto handler = [&](const drogon::HttpRequestPtr& req,std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
      auto cb = std::move(callback);
      auto send = [cb](int code,const json& value) {
        auto response = drogon::HttpResponse::newHttpResponse(); response->setStatusCode(static_cast<drogon::HttpStatusCode>(code));
        response->setContentTypeCode(drogon::CT_APPLICATION_JSON); response->setBody(value.dump());
        response->addHeader("Cache-Control","no-store"); response->addHeader("X-Content-Type-Options","nosniff"); cb(response);
      };
      if (!pool.submit([&,req,send] {
        int status = 200;
        try { auto result = service.handle(req,status); send(status,result); }
        catch (const Error& error) {
          int code = 400;
          if (error.code == "unauthorized") code=401; else if(error.code == "forbidden") code=403;
          else if(error.code == "not_found") code=404; else if(error.code == "stale_approval" || error.code == "idempotency_conflict" || error.code == "invalid_state" || error.code == "expired_approval") code=409;
          else if(error.code.starts_with("database")) code=503;
          send(code,{{"error",{{"code",error.code},{"message",error.what()}}}});
        } catch(const std::exception&) { send(400,{{"error",{{"code","invalid_request"},{"message","Invalid request or service failure."}}}}); }
      })) send(503,{{"error",{{"code","busy"},{"message","Queue is full. Retry later."}}}});
    };
    drogon::app().registerHandlerViaRegex("^/(api/.*|health/.*)$",handler,{drogon::Get,drogon::Post,drogon::Patch});
    const auto web = env("WEB_ROOT","apps/web/build");
    drogon::app().registerHandler("/",[web](const drogon::HttpRequestPtr&,std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
      auto response = drogon::HttpResponse::newFileResponse(web+"/200.html"); response->addHeader("X-Frame-Options","DENY"); response->addHeader("X-Content-Type-Options","nosniff"); cb(response);
    },{drogon::Get});
    drogon::app().getLoop()->runEvery(0.5,[&] {
      if (polling.exchange(true)) return;
      if (!pool.submit([&] { try { process_job(service.database,service.tenant,*service.ui_schema); } catch(const std::exception&) { /* Readiness exposes DB failures; do not log provider payloads. */ } polling = false; })) polling = false;
    });
    drogon::app().setClientMaxBodySize(300*1024).setDocumentRoot(web).setThreadNum(2)
      .addListener(env("BIND_ADDRESS","127.0.0.1"),static_cast<std::uint16_t>(std::stoi(env("PORT","8080")))).run();
  } catch(const std::exception& error) { std::cerr << "Runtime startup failed: " << error.what() << '\n'; return 1; }
}
