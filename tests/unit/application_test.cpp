#include "runtime/application.hpp"
#include "runtime/storage.hpp"
#include <iostream>
#include <fstream>
#include <random>
using namespace runtime::apps;
int passed=0;
void check(bool result) {if(!result) throw std::runtime_error("Assertion failed");}
void test(const std::string& name,const std::function<void()>& fn) {
  try {fn(); std::cout<<"PASS "<<name<<'\n'; ++passed;}
  catch(const std::exception& e) {std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n'; std::exit(1);}
}
void rejects(const std::function<void()>& fn) {try{fn();}catch(const std::exception&){return;} throw std::runtime_error("Expected rejection");}
j::value base() {return parse(R"({"schema_version":1,"key":"studio","name":"Studio","roles":["owner","artist","viewer"],"entities":[{"key":"projects","label":"Projects","fields":[{"key":"title","label":"Title","type":"text","required":true},{"key":"status","label":"Status","type":"select","options":["draft","ready"]},{"key":"image","label":"Image","type":"file"}],"access":{"read":["owner","artist","viewer"],"write":["owner","artist"]}}],"pages":[{"key":"projects","title":"Projects","entity":"projects","actions":["paint"]}],"actions":[{"key":"paint","label":"Paint","entity":"projects","capability":"media.paint","version":1,"contract":"record-in/result-out@1"}]})");}
j::object& entity(j::value& v) {return v.as_object().at("entities").as_array()[0].as_object();}
j::array& fields(j::value& v) {return entity(v).at("fields").as_array();}
j::object& action(j::value& v) {return v.as_object().at("actions").as_array()[0].as_object();}
int main() {
  test("application compiles without paint",[]{Application app(base()); auto view=app.describe(builtins(),"owner"); check(view.at("pages").as_array().size()==1); check(!view.at("capabilities").as_array()[0].as_object().at("available").as_bool());});
  test("missing capability does not block records",[]{Application app(base()); check(app.validate_record("projects",j::object{{"title","Painting project"}}).size()==1);});
  test("missing capability cannot pretend success",[]{auto v=base(); rejects([&]{builtins().invoke(action(v),j::object{});});});
  test("installing compatible capability unlocks existing app",[]{auto v=base(); Application app(v); auto registry=builtins(); registry.install({"media.paint",1,"record-in/result-out@1",true,[](const j::value&){return j::object{{"test_only",true}};}}); check(registry.resolve(action(v)).at("available").as_bool()); check(registry.invoke(action(v),j::object{}).as_object().at("test_only").as_bool()); check(app.spec==v.as_object());});
  test("wrong capability version does not unlock",[]{auto v=base(); auto registry=builtins(); registry.install({"media.paint",2,"record-in/result-out@1",true,[](const auto& in){return in;}}); check(!registry.resolve(action(v)).at("available").as_bool());});
  test("wrong capability contract does not unlock",[]{auto v=base(); auto registry=builtins(); registry.install({"media.paint",1,"different@1",true,[](const auto& in){return in;}}); check(!registry.resolve(action(v)).at("available").as_bool());});
  test("available capability needs implementation",[]{auto registry=builtins(); rejects([&]{registry.install({"broken",1,"record-in/result-out@1",true,{}});});});
  test("capability version cannot be overwritten",[]{auto registry=builtins(); rejects([&]{registry.install({"core.records.snapshot",1,"record-in/result-out@1",false,{}});});});
  test("application cannot mark capability installed",[]{auto v=base(); action(v)["installed"]=true; rejects([&]{Application app(v);});});
  test("application cannot inject code",[]{auto v=base(); v.as_object()["javascript"]="alert(1)"; rejects([&]{Application app(v);});});
  test("application rejects SQL field",[]{auto v=base(); fields(v)[0].as_object()["sql"]="DROP TABLE users"; rejects([&]{Application app(v);});});
  test("duplicate entity rejected",[]{auto v=base(); auto duplicate=entity(v); v.as_object()["entities"].as_array().push_back(duplicate); rejects([&]{Application app(v);});});
  test("duplicate field rejected",[]{auto v=base(); auto f=fields(v)[0]; fields(v).push_back(f); rejects([&]{Application app(v);});});
  test("unknown page entity rejected",[]{auto v=base(); v.as_object()["pages"].as_array()[0].as_object()["entity"]="missing"; rejects([&]{Application app(v);});});
  test("unknown page action rejected",[]{auto v=base(); v.as_object()["pages"].as_array()[0].as_object()["actions"]=j::array{"unknown"}; rejects([&]{Application app(v);});});
  test("unknown reference target rejected",[]{auto v=base(); fields(v).push_back(j::object{{"key","client"},{"label","Client"},{"type","reference"},{"entity","unknown"}}); rejects([&]{Application app(v);});});
  test("viewer cannot write",[]{Application app(base()); app.authorize("projects","viewer",false); rejects([&]{app.authorize("projects","viewer",true);});});
  test("custom artist role can write",[]{Application app(base()); app.authorize("projects","artist",true);});
  test("unknown role has no access",[]{Application app(base()); rejects([&]{app.authorize("projects","intruder",false);});});
  test("hidden pages removed by read permissions",[]{auto v=base(); entity(v)["access"].as_object()["read"]=j::array{"owner","artist"}; check(Application(v).describe(builtins(),"viewer").at("pages").as_array().empty());});
  test("owner access cannot disappear",[]{auto v=base(); entity(v)["access"].as_object()["write"]=j::array{"artist"}; rejects([&]{Application app(v);});});
  test("required record field enforced",[]{Application app(base()); rejects([&]{app.validate_record("projects",j::object{});});});
  test("unknown record field rejected",[]{Application app(base()); rejects([&]{app.validate_record("projects",j::object{{"title","x"},{"workspace_id","foreign"}});});});
  test("select is constrained",[]{Application app(base()); rejects([&]{app.validate_record("projects",j::object{{"title","x"},{"status","invented"}});});});
  test("file references must be opaque IDs",[]{Application app(base()); rejects([&]{app.validate_record("projects",j::object{{"title","x"},{"image","../../etc/passwd"}});});});
  test("optional null allowed",[]{Application app(base()); app.validate_record("projects",j::object{{"title","x"},{"image",nullptr}});});
  test("required null rejected",[]{Application app(base()); rejects([&]{app.validate_record("projects",j::object{{"title",nullptr}});});});
  test("exact decimal preserves precision",[]{auto v=base(); fields(v).push_back(j::object{{"key","amount"},{"label","Amount"},{"type","decimal"}}); Application app(v); auto data=app.validate_record("projects",j::object{{"title","x"},{"amount","9007199254740993.001"}}); check(text(data.at("amount"))=="9007199254740993.001"); rejects([&]{app.validate_record("projects",j::object{{"title","x"},{"amount",1.5}});});});
  test("safe integers only",[]{check(integer(j::value(42))==42); rejects([]{integer(j::value(9007199254740992LL));});});
  test("valid and invalid dates",[]{auto v=base(); fields(v).push_back(j::object{{"key","due"},{"label","Due"},{"type","date"}}); Application app(v); app.validate_record("projects",j::object{{"title","x"},{"due","2024-02-29"}}); rejects([&]{app.validate_record("projects",j::object{{"title","x"},{"due","2025-02-29"}});});});
  test("additive schema update preserves records",[]{auto v=base(); fields(v).push_back(j::object{{"key","notes"},{"label","Notes"},{"type","multiline"}}); Application updated(v); updated.compatible_with(Application(base())); updated.validate_record("projects",j::object{{"title","existing"}});});
  test("field deletion requires migration",[]{auto v=base(); fields(v).erase(fields(v).begin()+2); rejects([&]{Application(v).compatible_with(Application(base()));});});
  test("type change requires migration",[]{auto v=base(); fields(v)[0].as_object()["type"]="integer"; rejects([&]{Application(v).compatible_with(Application(base()));});});
  test("new required field requires backfill",[]{auto v=base(); fields(v).push_back(j::object{{"key","new_field"},{"label","New"},{"type","text"},{"required",true}}); rejects([&]{Application(v).compatible_with(Application(base()));});});
  test("removing select option requires migration",[]{auto v=base(); fields(v)[1].as_object()["options"]=j::array{"draft"}; rejects([&]{Application(v).compatible_with(Application(base()));});});
  test("adding select option is compatible",[]{auto v=base(); fields(v)[1].as_object()["options"].as_array().push_back("archived"); Application(v).compatible_with(Application(base()));});
  test("bounded JSON nesting",[]{rejects([]{parse(std::string(40,'[')+"0"+std::string(40,']'));});});
  test("bounded JSON length",[]{rejects([]{parse("12345",4);});});
  test("binary lossless roundtrip",[]{std::string data("\0\1\xff\n",4); check(decode(encode(data))==data);});
  test("empty object roundtrip",[]{check(decode(encode(""))=="");});
  test("compressible data uses zstd",[]{std::string data(100000,'a'); auto encoded=encode(data); check(encoded.codec=="zstd"); check(encoded.bytes.size()<data.size()/10); check(decode(encoded)==data);});
  test("random data not expanded",[]{std::mt19937 random(42); std::string data(10000,' '); for(auto& c:data)c=static_cast<char>(random()); auto encoded=encode(data); check(encoded.codec=="identity"); check(decode(encoded)==data);});
  test("corruption rejected",[]{auto object=encode("original"); object.bytes[0]='X'; rejects([&]{decode(object);});});
  test("truncated zstd rejected",[]{auto object=encode(std::string(10000,'x')); object.bytes.pop_back(); rejects([&]{decode(object);});});
  test("concatenated frame rejected",[]{auto object=encode(std::string(10000,'x')); object.bytes+=object.bytes; rejects([&]{decode(object);});});
  test("oversized decoded metadata rejected",[]{auto object=encode("x"); object.original_size=max_asset_bytes+1; rejects([&]{decode(object);});});
  test("wrong decoded size rejected",[]{auto object=encode(std::string(10000,'x')); object.original_size=5000; rejects([&]{decode(object);});});
  test("unknown codec rejected",[]{auto object=encode("x"); object.codec="executable"; rejects([&]{decode(object);});});
  test("upload size limit",[]{rejects([]{encode(std::string(max_asset_bytes+1,'x'));});});
  test("path and header injection rejected",[]{rejects([]{safe_filename("../data");}); rejects([]{safe_filename("file\r\nHeader: x");});});
  test("valid filename retained",[]{check(safe_filename("my artwork.png")=="my artwork.png");});
  test("hex codec and digest",[]{check(unhex(hex("hello"))=="hello"); check(hash("hello")=="2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824"); rejects([]{unhex("xx");});});
  test("independent app definitions use the same engine",[]{for(const auto& name:{"bookings","tickets","experiments","inventory","lessons","shipments","manuscripts","campaigns","cases","sessions"}){auto v=base(); v.as_object()["key"]=name; entity(v)["key"]=name; v.as_object()["pages"].as_array()[0].as_object()["entity"]=name; action(v)["entity"]=name; Application app(v); app.validate_record(name,j::object{{"title","Untemplated record"}}); check(app.describe(builtins(),"owner").at("pages").as_array().size()==1);}});
  std::cout<<passed<<" application/capability/storage tests passed\n";
}
