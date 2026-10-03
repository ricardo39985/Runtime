#include "runtime/application.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <set>

namespace runtime::apps {
void need(bool condition, std::string_view code, std::string_view message) {
  if (!condition) throw Fault(std::string(code),std::string(message));
}
j::value parse(std::string_view input, std::size_t limit) {
  need(input.size() <= limit,"input_limit","Input exceeds the supported size.");
  j::parse_options options; options.max_depth = 24;
  boost::system::error_code error;
  auto result = j::parse(input, error, {}, options);
  need(!error,"invalid_json","Invalid or excessively nested JSON."); return result;
}
std::string text(const j::value& v) { need(v.is_string(),"invalid_spec","Expected text."); return std::string(v.as_string()); }
std::string str(const j::object& v,std::string_view key) {
  const auto* item=v.if_contains(key); need(item != nullptr,"invalid_spec","Required property is missing."); return text(*item);
}
std::int64_t integer(const j::value& v) {
  need(v.is_int64() || (v.is_uint64() && v.as_uint64() <= 9007199254740991ULL),"invalid_value","Expected a safe integer.");
  auto n = v.is_int64() ? v.as_int64() : static_cast<std::int64_t>(v.as_uint64());
  need(n >= -9007199254740991LL && n <= 9007199254740991LL,"invalid_value","Integer exceeds exact browser precision; use decimal text."); return n;
}
bool identifier(std::string_view v) {
  return !v.empty() && v.size() <= 64 && v[0] >= 'a' && v[0] <= 'z'
    && v.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") == std::string_view::npos;
}
void keys(const j::object& v,std::initializer_list<std::string_view> allowed) {
  for(const auto& item:v) need(std::find(allowed.begin(),allowed.end(),std::string_view(item.key())) != allowed.end(),"invalid_spec","Unknown property; executable or unsupported specification rejected.");
}
namespace {
const j::array& array(const j::object& v,std::string_view key,std::size_t min,std::size_t max) {
  const auto* found=v.if_contains(key);
  need(found && found->is_array(),"invalid_spec","Expected a bounded list.");
  const auto& result=found->as_array(); need(result.size() >= min && result.size() <= max,"invalid_spec","List size outside limits."); return result;
}
const j::object& object(const j::value& v) { need(v.is_object(),"invalid_spec","Expected an object."); return v.as_object(); }
void label(const j::object& v,std::string_view key,std::size_t max=160) {
  auto value=str(v,key); need(!value.empty() && value.size() <= max && value.find('\0') == std::string::npos,"invalid_spec","Invalid label.");
}
bool required(const j::object& field) {
  auto* p=field.if_contains("required"); need(!p || p->is_bool(),"invalid_spec","required must be boolean."); return p && p->as_bool();
}
bool contains(const j::array& list,std::string_view value) {
  return std::any_of(list.begin(),list.end(),[&](const auto& item){return item.is_string() && item.as_string() == value;});
}
void roles(const j::array& selected,const j::array& defined) {
  std::set<std::string> seen;
  for(const auto& v:selected) {auto r=text(v); need(contains(defined,r) && seen.insert(r).second,"invalid_spec","Role is unknown or duplicated.");}
}
const j::object* lookup(const j::array& entries,std::string_view key) {
  for(const auto& value:entries) { if(str(object(value),"key") == key) return &value.as_object(); }
  return nullptr;
}
bool hex_id(std::string_view value) {
  return value.size()==32 && value.find_first_not_of("0123456789abcdef")==std::string_view::npos;
}
void field_value(const j::object& field,const j::value& value) {
  const auto type=str(field,"type");
  if(value.is_null()) {need(!required(field),"invalid_record","A required field is empty."); return;}
  if(type=="boolean") {need(value.is_bool(),"invalid_record","Expected true or false."); return;}
  if(type=="integer") {integer(value); return;}
  auto s=text(value); need(s.size() <= 16384 && s.find('\0') == std::string::npos,"invalid_record","Field text exceeds limits or contains a NUL byte.");
  need(!required(field) || !s.empty(),"invalid_record","A required field is empty.");
  if(type=="select") need(contains(field.at("options").as_array(),s),"invalid_record","Value is not an allowed option.");
  if(type=="reference" || type=="file") need(hex_id(s),"invalid_record","Expected a record or file identifier.");
  if(type=="decimal") {
    std::size_t start=s.starts_with('-') ? 1 : 0;
    const auto dot=s.find('.',start); auto whole=s.substr(start,dot==s.npos ? s.size()-start : dot-start);
    need(!whole.empty() && whole.find_first_not_of("0123456789")==s.npos && s.size() <= 80,"invalid_record","Expected exact decimal text.");
    if(dot!=s.npos) {auto fraction=s.substr(dot+1); need(!fraction.empty() && fraction.find_first_not_of("0123456789")==s.npos,"invalid_record","Expected exact decimal text.");}
  }
  if(type=="date") {
    need(s.size()==10 && s[4]=='-' && s[7]=='-' && (s.substr(0,4)+s.substr(5,2)+s.substr(8,2)).find_first_not_of("0123456789")==s.npos,"invalid_record","Expected YYYY-MM-DD.");
    auto date=std::chrono::year_month_day{std::chrono::year(std::stoi(s.substr(0,4))),std::chrono::month(static_cast<unsigned>(std::stoi(s.substr(5,2)))),std::chrono::day(static_cast<unsigned>(std::stoi(s.substr(8,2))))};
    need(date.ok() && int(date.year()) >= 1900,"invalid_record","Invalid calendar date.");
  }
}
}
Application::Application(const j::value& input) : spec(object(input)) {
  need(j::serialize(spec).size() <= 256*1024,"input_limit","Application definition is too large.");
  keys(spec,{"schema_version","key","name","description","roles","entities","pages","actions"});
  need(integer(spec.at("schema_version"))==1,"unsupported_schema","Unsupported application schema version.");
  need(identifier(str(spec,"key")),"invalid_spec","Invalid application key."); label(spec,"name");
  if(spec.contains("description")) need(text(spec.at("description")).size() <= 4000,"invalid_spec","Description too long.");
  const auto& defined=array(spec,"roles",1,32); need(contains(defined,"owner"),"invalid_spec","An owner role is required.");
  std::set<std::string> names;
  for(const auto& r:defined) need(identifier(text(r)) && names.insert(text(r)).second,"invalid_spec","Invalid or duplicate role.");
  const auto& entities=array(spec,"entities",1,64); names.clear();
  const std::set<std::string> types{"text","multiline","integer","decimal","boolean","date","select","reference","file"};
  for(const auto& value:entities) {
    const auto& e=object(value); keys(e,{"key","label","fields","access"}); auto key=str(e,"key");
    need(identifier(key) && names.insert(key).second,"invalid_spec","Invalid or duplicate entity."); label(e,"label");
    const auto& access=object(e.at("access")); keys(access,{"read","write"});
    roles(array(access,"read",1,32),defined); roles(array(access,"write",1,32),defined);
    need(contains(access.at("read").as_array(),"owner") && contains(access.at("write").as_array(),"owner"),"invalid_spec","Owner must retain access.");
    for(const auto& role:access.at("write").as_array()) need(contains(access.at("read").as_array(),text(role)),"invalid_spec","Writers must also have read access.");
    std::set<std::string> fields;
    for(const auto& f:array(e,"fields",1,64)) {
      const auto& field=object(f); keys(field,{"key","label","type","required","options","entity"}); auto fk=str(field,"key"), type=str(field,"type");
      need(identifier(fk) && fk!="id" && fk!="version" && fields.insert(fk).second,"invalid_spec","Invalid, reserved or duplicate field key."); label(field,"label");
      need(types.contains(type),"unsupported_field","Unsupported field type."); required(field);
      if(type=="select") {std::set<std::string> options; for(const auto& o:array(field,"options",1,100)) {auto option=text(o); need(!option.empty() && option.size()<=160 && options.insert(option).second,"invalid_spec","Invalid or duplicate option.");}}
      else need(!field.contains("options"),"invalid_spec","Only select fields have options.");
      if(type=="reference") need(lookup(entities,str(field,"entity"))!=nullptr,"invalid_reference","Unknown referenced entity.");
      else need(!field.contains("entity"),"invalid_spec","Only reference fields have a target entity.");
    }
  }
  const auto& actions=array(spec,"actions",0,128); names.clear();
  for(const auto& a:actions) {
    const auto& action=object(a); keys(action,{"key","label","entity","capability","version","contract"}); auto key=str(action,"key");
    need(identifier(key) && names.insert(key).second,"invalid_spec","Invalid or duplicate action."); label(action,"label"); entity(str(action,"entity"));
    auto name=str(action,"capability"); need(!name.empty() && name.size()<=128 && name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_.")==name.npos,"invalid_spec","Invalid capability identifier.");
    need(integer(action.at("version"))>=1 && integer(action.at("version"))<=100000,"invalid_spec","Invalid capability version.");
    need(str(action,"contract")=="record-in/result-out@1","unsupported_contract","Capability port must use the supported record envelope contract.");
  }
  names.clear();
  for(const auto& p:array(spec,"pages",1,64)) {
    const auto& page=object(p); keys(page,{"key","title","entity","actions"}); auto key=str(page,"key");
    need(identifier(key) && names.insert(key).second,"invalid_spec","Invalid or duplicate page."); label(page,"title"); entity(str(page,"entity"));
    for(const auto& a:array(page,"actions",0,32)) need(str(action(text(a)),"entity")==str(page,"entity"),"invalid_reference","Action belongs to a different entity.");
  }
}
const j::object& Application::entity(std::string_view key) const {
  const auto* found=lookup(spec.at("entities").as_array(),key); need(found,"not_found","Entity not found."); return *found;
}
const j::object& Application::action(std::string_view key) const {
  const auto* found=lookup(spec.at("actions").as_array(),key); need(found,"not_found","Action not found."); return *found;
}
void Application::authorize(std::string_view key,std::string_view role,bool write) const {
  const auto& access=entity(key).at("access").as_object();
  need(contains(access.at(write?"write":"read").as_array(),role),"forbidden","Role does not have access to this collection.");
}
j::object Application::validate_record(std::string_view key,const j::value& input) const {
  const auto& data=object(input); const auto& fields=entity(key).at("fields").as_array();
  need(j::serialize(data).size()<=64*1024,"input_limit","Record exceeds 64 KiB.");
  for(const auto& item:data) need(lookup(fields,item.key()),"invalid_record","Record contains an unknown field.");
  for(const auto& f:fields) {const auto& field=f.as_object(); auto* v=data.if_contains(str(field,"key")); need(v || !required(field),"invalid_record","Required field is missing."); if(v) field_value(field,*v);}
  return data;
}
void Application::compatible_with(const Application& old) const {
  need(str(spec,"key")==str(old.spec,"key"),"migration_required","Application key cannot change.");
  for(const auto& value:old.spec.at("entities").as_array()) {
    const auto& before=value.as_object(); const auto* after=lookup(spec.at("entities").as_array(),str(before,"key"));
    need(after,"migration_required","Removing an entity requires an explicit data migration.");
    const auto& previous=before.at("fields").as_array(); const auto& current=after->at("fields").as_array();
    for(const auto& f:previous) {
      const auto& oldf=f.as_object(); const auto* newf=lookup(current,str(oldf,"key"));
      need(newf && str(*newf,"type")==str(oldf,"type"),"migration_required","Field removal or type change requires an explicit migration.");
      need(required(oldf) || !required(*newf),"migration_required","Making a field required needs an explicit backfill.");
      if(str(oldf,"type")=="reference") need(str(*newf,"entity")==str(oldf,"entity"),"migration_required","Changing a relationship needs a migration.");
      if(str(oldf,"type")=="select") for(const auto& option:oldf.at("options").as_array()) need(contains(newf->at("options").as_array(),text(option)),"migration_required","Removing an option needs a migration.");
    }
    for(const auto& f:current) if(!lookup(previous,str(f.as_object(),"key"))) need(!required(f.as_object()),"migration_required","A new required field needs an explicit backfill.");
  }
}
void Registry::install(Capability capability) {
  need(!capability.name.empty() && capability.version>0 && (!capability.available || bool(capability.invoke)),"invalid_capability","An available capability requires an implementation.");
  auto key=std::pair{capability.name,capability.version}; need(!entries_.contains(key),"capability_conflict","Capability versions are immutable."); entries_.emplace(std::move(key),std::move(capability));
}
j::object Registry::resolve(const j::object& r) const {
  auto name=str(r,"capability"); auto version=integer(r.at("version")); auto it=entries_.find({name,version});
  bool available=it!=entries_.end() && it->second.available && it->second.contract==str(r,"contract");
  return {{"capability",name},{"version",version},{"available",available},{"status",available?"ready":"capability_unavailable"},
          {"reason",available?"":(it==entries_.end()?"not_installed":(it->second.contract!=str(r,"contract")?"contract_mismatch":"not_configured"))}};
}
j::value Registry::invoke(const j::object& requirement,const j::value& input) const {
  need(resolve(requirement).at("available").as_bool(),"capability_unavailable","The application is intact; this action needs an installed, compatible capability.");
  return entries_.at({str(requirement,"capability"),integer(requirement.at("version"))}).invoke(input);
}
j::object Application::describe(const Registry& registry,std::string_view role) const {
  j::array pages, capabilities;
  for(const auto& p:spec.at("pages").as_array()) {
    const auto& page=p.as_object(); const auto& e=entity(str(page,"entity"));
    if(!contains(e.at("access").as_object().at("read").as_array(),role)) continue;
    j::array actions;
    for(const auto& a:page.at("actions").as_array()) {auto item=action(text(a)); item["resolution"]=registry.resolve(item); actions.push_back(item);}
    pages.push_back(j::object{{"key",page.at("key")},{"title",page.at("title")},{"entity",e},{"actions",actions},
      {"can_write",contains(e.at("access").as_object().at("write").as_array(),role)}});
  }
  for(const auto& a:spec.at("actions").as_array()) {auto resolution=registry.resolve(a.as_object()); resolution["action"]=a.as_object().at("key"); capabilities.push_back(resolution);}
  return {{"name",spec.at("name")},{"pages",pages},{"capabilities",capabilities},{"files_enabled",true},{"storage_codec","adaptive-zstd-v1"}};
}
Registry builtins() {
  Registry registry;
  registry.install({"core.records.snapshot",1,"record-in/result-out@1",true,[](const j::value& input){return j::object{{"kind","record_snapshot"},{"record",input}};}});
  return registry;
}
} // namespace runtime::apps
