#pragma once
#include "runtime/application.hpp"
#include <nlohmann/json.hpp>
namespace runtime::application {
using Json = nlohmann::json;
inline void keys(const Json& object, std::initializer_list<const char*> allowed, std::initializer_list<const char*> required = {}) {
  ensure(object.is_object(), "invalid_spec", "Expected a JSON object.");
  for (const auto& [key, unused] : object.items()) { (void)unused; ensure(std::find(allowed.begin(), allowed.end(), key) != allowed.end(), "invalid_spec", "Unknown property: " + key); }
  for (const auto* key : required) ensure(object.contains(key), "invalid_spec", "Missing property: " + std::string(key));
}
inline Json read_document(std::string_view text, std::size_t limit = 512 * 1024) {
  ensure(text.size() <= limit, "document_limit", "Document exceeds its size limit.");
  std::vector<std::set<std::string>> objects;
  return Json::parse(text, [&](int depth, Json::parse_event_t event, Json& parsed) {
    ensure(depth <= 24, "document_limit", "Document nesting exceeds its limit.");
    if (event == Json::parse_event_t::object_start) objects.emplace_back();
    if (event == Json::parse_event_t::key) ensure(objects.back().insert(parsed.get<std::string>()).second, "invalid_spec", "Duplicate JSON property.");
    if (event == Json::parse_event_t::object_end) objects.pop_back();
    return true;
  });
}
inline Value value_from_json(const Json& value) {
  if (value.is_null()) return {};
  if (value.is_boolean()) return value.get<bool>();
  ensure(value.is_string(), "invalid_type", "Data values must be strings, booleans or null. Integers use exact decimal strings.");
  return value.get<std::string>();
}
inline Json value_to_json(const Value& v) {
  if (std::holds_alternative<std::monostate>(v)) return nullptr;
  if (const auto* s = std::get_if<std::string>(&v)) return *s;
  return std::get<bool>(v);
}
inline Record record_from_json(const Json& object) {
  ensure(object.is_object() && object.size() <= 64, "invalid_record", "Expected a bounded record object.");
  Record result; for (const auto& [key, value] : object.items()) result.emplace(key, value_from_json(value)); return result;
}
inline Json record_to_json(const Record& record) {
  auto result = Json::object(); for (const auto& [key, value] : record) result[key] = value_to_json(value); return result;
}
inline std::set<std::string> role_set(const Json& value) {
  ensure(value.is_array() && value.size() <= 24, "invalid_role", "Expected a bounded role list.");
  std::set<std::string> result; for (const auto& item : value) ensure(result.insert(item.get<std::string>()).second, "invalid_role", "Duplicate role."); return result;
}
inline std::vector<Field> fields_from_json(const Json& values) {
  ensure(values.is_array() && values.size() <= 64, "schema_limit", "Expected at most 64 fields.");
  const std::map<std::string,Kind> kinds{{"text",Kind::text},{"integer",Kind::integer},{"boolean",Kind::boolean},{"date",Kind::date},{"choice",Kind::choice},{"reference",Kind::reference},{"file",Kind::file}};
  std::vector<Field> result;
  for (const auto& v : values) {
    keys(v,{"id","label","type","required","unique","max_length","options","target","default"},{"id","label","type"});
    const auto kind = v.at("type").get<std::string>(); ensure(kinds.contains(kind),"unknown_field_type","Field type is not installed.");
    Field f; f.id=v.at("id"); f.label=v.at("label"); f.kind=kinds.at(kind); f.required=v.value("required",false); f.unique=v.value("unique",false);
    if(v.contains("max_length")) { ensure(v.at("max_length").is_number_unsigned(),"invalid_field","max_length must be positive."); f.max_length=v.at("max_length").get<std::size_t>(); }
    f.options=v.value("options",std::vector<std::string>{}); f.target=v.value("target",std::string{});
    if(v.contains("default")) { f.default_value=value_from_json(v.at("default")); }
    result.push_back(std::move(f));
  }
  fields_valid(result); return result;
}
inline Spec spec_from_json(const Json& v) {
  keys(v,{"schema_version","name","description","roles","entities","pages","capabilities","actions","storage"},{"schema_version","name","roles","entities","pages","capabilities","actions"});
  ensure(v.at("schema_version")==1,"unsupported_version","Unsupported ApplicationSpec version.");
  Spec app; app.name=v.at("name"); app.description=v.value("description",std::string{}); app.roles=role_set(v.at("roles"));
  for (const auto* key : {"entities","pages","capabilities","actions"}) ensure(v.at(key).is_array() && v.at(key).size() <= 64,"schema_limit","Expected a bounded array.");
  for(const auto& e:v.at("entities")) {
    keys(e,{"id","label","fields","read_roles","write_roles"},{"id","label","fields","read_roles","write_roles"});
    app.entities.push_back({e.at("id"),e.at("label"),fields_from_json(e.at("fields")),role_set(e.at("read_roles")),role_set(e.at("write_roles"))});
  }
  for(const auto& c:v.at("capabilities")) {
    keys(c,{"id","description","inputs","outputs"},{"id","description","inputs","outputs"});
    app.capabilities.push_back({c.at("id"),c.at("description"),fields_from_json(c.at("inputs")),fields_from_json(c.at("outputs"))});
  }
  for(const auto& a:v.at("actions")) {
    keys(a,{"id","label","entity","capability","inputs","outputs","roles"},{"id","label","entity","capability","inputs","outputs","roles"});
    app.actions.push_back({a.at("id"),a.at("label"),a.at("entity"),a.at("capability"),a.at("inputs").get<std::map<std::string,std::string>>(),a.at("outputs").get<std::map<std::string,std::string>>(),role_set(a.at("roles"))});
  }
  for(const auto& p:v.at("pages")) {
    keys(p,{"id","title","blocks"},{"id","title","blocks"});
    ensure(p.at("blocks").is_array() && p.at("blocks").size()<=32,"schema_limit","Invalid page blocks.");
    Page page{p.at("id"),p.at("title"),{}};
    for(const auto& b:p.at("blocks")) {
      keys(b,{"kind","title","entity","action","fields"},{"kind"});
      page.blocks.push_back({b.at("kind"),b.value("title",std::string{}),b.value("entity",std::string{}),b.value("action",std::string{}),b.value("fields",std::vector<std::string>{})});
    }
    app.pages.push_back(std::move(page));
  }
  if(v.contains("storage")) {
    const auto& storage=v.at("storage"); keys(storage,{"read_roles","write_roles","max_file_bytes","max_space_bytes"},{"read_roles","write_roles"});
    app.storage.read_roles=role_set(storage.at("read_roles")); app.storage.write_roles=role_set(storage.at("write_roles"));
    for(const auto* key:{"max_file_bytes","max_space_bytes"}) if(storage.contains(key)) ensure(storage.at(key).is_number_unsigned(),"storage_limit","Storage limits must be positive integers.");
    app.storage.max_file_bytes=storage.value("max_file_bytes",std::size_t{8*1024*1024}); app.storage.max_space_bytes=storage.value("max_space_bytes",std::size_t{64*1024*1024});
  }
  validate(app); return app;
}
inline Json capability_report(const Spec& app,const Registry& registry) {
  auto result=Json::array();
  for(const auto& c:app.capabilities) result.push_back({{"id",c.id},{"description",c.description},{"status",availability(c,registry)}});
  return result;
}
}
