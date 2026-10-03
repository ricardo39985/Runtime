#pragma once
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace runtime::application {
struct Invalid : std::runtime_error {
  std::string code;
  Invalid(std::string c, std::string message) : std::runtime_error(std::move(message)), code(std::move(c)) {}
};
inline void ensure(bool yes, std::string_view code, std::string_view message) {
  if (!yes) throw Invalid(std::string(code), std::string(message));
}
inline bool identifier(std::string_view id) {
  return !id.empty() && id.size() <= 48 && id.front() >= 'a' && id.front() <= 'z'
    && id.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") == std::string_view::npos
    && id != "__proto__" && id != "constructor" && id != "prototype";
}
inline bool capability_id(std::string_view id) {
  auto at = id.find('@');
  return at != std::string_view::npos && at > 0 && at + 1 < id.size() && id.size() <= 120
    && id.substr(0, at).find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789._-") == std::string_view::npos
    && id.substr(at + 1).find_first_not_of("0123456789") == std::string_view::npos;
}
using Value = std::variant<std::monostate, std::string, bool>;
using Record = std::map<std::string, Value>;
enum class Kind { text, integer, boolean, date, choice, reference, file };
struct Field {
  std::string id, label;
  Kind kind = Kind::text;
  bool required = false, unique = false;
  std::size_t max_length = 4096;
  std::vector<std::string> options;
  std::string target;
  std::optional<Value> default_value;
};
struct Entity {
  std::string id, label;
  std::vector<Field> fields;
  std::set<std::string> read_roles{"owner"}, write_roles{"owner"};
};
struct Requirement { std::string id, description; std::vector<Field> inputs, outputs; };
struct Action {
  std::string id, label, entity, capability;
  std::map<std::string, std::string> inputs, outputs;
  std::set<std::string> roles{"owner"};
};
struct Block {
  std::string kind, title, entity, action;
  std::vector<std::string> fields;
};
struct Page { std::string id, title; std::vector<Block> blocks; };
struct StoragePolicy {
  std::set<std::string> read_roles{"owner"}, write_roles{"owner"};
  std::size_t max_file_bytes=8*1024*1024, max_space_bytes=64*1024*1024;
};
struct Spec {
  std::string name, description;
  std::set<std::string> roles{"owner"};
  std::vector<Entity> entities;
  std::vector<Page> pages;
  std::vector<Requirement> capabilities;
  std::vector<Action> actions;
  StoragePolicy storage;
};
inline const Field& field(const std::vector<Field>& fields, const std::string& id) {
  auto it = std::find_if(fields.begin(), fields.end(), [&](const Field& f) { return f.id == id; });
  ensure(it != fields.end(), "unknown_field", "A referenced field does not exist: " + id); return *it;
}
inline const Entity& entity(const Spec& app, const std::string& id) {
  auto it = std::find_if(app.entities.begin(), app.entities.end(), [&](const Entity& e) { return e.id == id; });
  ensure(it != app.entities.end(), "unknown_entity", "A referenced entity does not exist: " + id); return *it;
}
inline const Requirement& requirement(const Spec& app, const std::string& id) {
  auto it = std::find_if(app.capabilities.begin(), app.capabilities.end(), [&](const Requirement& r) { return r.id == id; });
  ensure(it != app.capabilities.end(), "unknown_capability", "Action references an undeclared capability: " + id); return *it;
}
inline const Action& action(const Spec& app, const std::string& id) {
  auto it = std::find_if(app.actions.begin(), app.actions.end(), [&](const Action& a) { return a.id == id; });
  ensure(it != app.actions.end(), "unknown_action", "A referenced action does not exist: " + id); return *it;
}
inline std::int64_t integer(std::string_view value) {
  ensure(!value.empty() && value.size() <= 20 && value.front() != '+', "invalid_integer", "Use a canonical signed integer string.");
  std::int64_t parsed = 0;
  const auto [end, ec] = std::from_chars(value.data(), value.data() + value.size(), parsed);
  ensure(ec == std::errc{} && end == value.data() + value.size() && std::to_string(parsed) == value,
         "invalid_integer", "Use a canonical signed 64-bit integer string."); return parsed;
}
inline void validate_date(std::string_view text) {
  ensure(text.size() == 10 && text[4] == '-' && text[7] == '-', "invalid_date", "Use YYYY-MM-DD.");
  auto part = [&](std::size_t begin, std::size_t length) {
    auto s = text.substr(begin, length); unsigned out = 0;
    ensure(s.find_first_not_of("0123456789") == std::string_view::npos, "invalid_date", "Invalid date.");
    auto [last, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
    ensure(ec == std::errc{} && last == s.data() + s.size(), "invalid_date", "Invalid date."); return out;
  };
  const auto year = part(0, 4);
  std::chrono::year_month_day day{std::chrono::year(static_cast<int>(year)), std::chrono::month(part(5, 2)), std::chrono::day(part(8, 2))};
  ensure(year >= 1 && day.ok(), "invalid_date", "Invalid calendar date.");
}
inline void validate_value(const Field& f, const Value& value) {
  if (std::holds_alternative<std::monostate>(value)) { ensure(!f.required, "required_field", "Missing required field: " + f.id); return; }
  if (f.kind == Kind::boolean) { ensure(std::holds_alternative<bool>(value), "invalid_type", "Expected boolean: " + f.id); return; }
  ensure(std::holds_alternative<std::string>(value), "invalid_type", "Expected string: " + f.id);
  const auto& text = std::get<std::string>(value);
  ensure(text.size() <= f.max_length && text.find('\0') == std::string::npos, "field_limit", "Invalid or oversized field: " + f.id);
  ensure(!f.required || !text.empty(), "required_field", "Empty required field: " + f.id);
  if (f.kind == Kind::integer) (void)integer(text);
  if (f.kind == Kind::date) validate_date(text);
  if (f.kind == Kind::choice) ensure(std::find(f.options.begin(), f.options.end(), text) != f.options.end(), "invalid_choice", "Unknown choice: " + f.id);
  if (f.kind == Kind::reference || f.kind == Kind::file)
    ensure(text.size() == 32 && text.find_first_not_of("0123456789abcdef") == std::string::npos, "invalid_reference", "Expected an opaque record or asset ID.");
}
inline Record normalize(const std::vector<Field>& fields, const Record& input) {
  ensure(input.size() <= 64, "record_limit", "Too many record fields.");
  Record out;
  for (const auto& [key, value] : input) { const auto& f = field(fields, key); validate_value(f, value); out[key] = value; }
  for (const auto& f : fields) if (!out.contains(f.id)) {
    Value value = f.default_value.value_or(Value{}); validate_value(f, value); out[f.id] = value;
  }
  return out;
}
inline void fields_valid(const std::vector<Field>& fields) {
  ensure(fields.size() <= 64, "schema_limit", "At most 64 fields per object.");
  std::set<std::string> ids;
  for (const auto& f : fields) {
    ensure(identifier(f.id) && ids.insert(f.id).second, "invalid_field", "Field IDs must be safe and unique.");
    ensure(!f.label.empty() && f.label.size() <= 160 && f.max_length >= 1 && f.max_length <= 16384, "invalid_field", "Invalid field bounds.");
    ensure(!f.unique || (f.max_length <= 256 && f.kind != Kind::boolean), "invalid_unique", "Unique fields require at most 256 bytes and cannot be boolean.");
    ensure(f.options.size() <= 64, "schema_limit", "Too many choices.");
    if (f.kind == Kind::choice) {
      ensure(!f.options.empty(), "invalid_choice", "A choice field needs choices.");
      ensure(std::set<std::string>(f.options.begin(), f.options.end()).size() == f.options.size(), "invalid_choice", "Duplicate choices.");
      for (const auto& v : f.options) ensure(!v.empty() && v.size() <= f.max_length, "invalid_choice", "Invalid choice value.");
    } else ensure(f.options.empty(), "invalid_field", "Options belong only to choice fields.");
    if (f.kind == Kind::reference) ensure(identifier(f.target), "invalid_reference", "Reference needs a target entity.");
    else ensure(f.target.empty(), "invalid_field", "Only reference fields have a target.");
    if (f.default_value) {
      ensure(f.kind != Kind::reference && f.kind != Kind::file, "invalid_default", "References cannot have global defaults.");
      validate_value(f, *f.default_value);
    }
  }
}
inline void roles_valid(const std::set<std::string>& selected, const std::set<std::string>& available) {
  for (const auto& role : selected) ensure(available.contains(role), "unknown_role", "Undeclared application role: " + role);
}
inline void authorize(const std::set<std::string>& roles, const std::string& role) {
  ensure(roles.contains(role), "forbidden", "The authenticated application role cannot perform this operation.");
}
inline void validate(const Spec& app) {
  ensure(!app.name.empty() && app.name.size() <= 160 && app.description.size() <= 4000, "invalid_app", "Invalid application name or description.");
  ensure(!app.entities.empty() && app.entities.size() <= 32 && !app.pages.empty() && app.pages.size() <= 32,
         "schema_limit", "Application requires 1–32 entities and 1–32 pages.");
  ensure(app.roles.contains("owner") && app.roles.size() <= 24, "invalid_role", "Application must declare owner and at most 24 roles.");
  for (const auto& r : app.roles) ensure(identifier(r), "invalid_role", "Invalid role ID.");
  roles_valid(app.storage.read_roles,app.roles); roles_valid(app.storage.write_roles,app.roles);
  ensure(app.storage.read_roles.contains("owner") && app.storage.write_roles.contains("owner"),"invalid_role","Owner must retain storage access.");
  for(const auto& role:app.storage.write_roles) ensure(app.storage.read_roles.contains(role),"invalid_role","Storage writers require read access.");
  ensure(app.storage.max_file_bytes>0 && app.storage.max_file_bytes<=8*1024*1024 && app.storage.max_space_bytes>=app.storage.max_file_bytes && app.storage.max_space_bytes<=1024*1024*1024,"storage_limit","Storage limits exceed the configured local platform bounds.");
  std::set<std::string> entities, pages, caps, actions;
  for (const auto& e : app.entities) {
    ensure(identifier(e.id) && entities.insert(e.id).second && !e.label.empty() && e.label.size() <= 160, "invalid_entity", "Entity IDs must be safe and unique.");
    ensure(!e.fields.empty(), "invalid_entity", "Entity requires fields."); fields_valid(e.fields);
    roles_valid(e.read_roles, app.roles); roles_valid(e.write_roles, app.roles);
    ensure(e.read_roles.contains("owner") && e.write_roles.contains("owner"), "invalid_role", "Application owner must retain data access.");
    for (const auto& r : e.write_roles) ensure(e.read_roles.contains(r), "invalid_role", "A write role also requires read access.");
  }
  for (const auto& e : app.entities) for (const auto& f : e.fields) if (f.kind == Kind::reference) (void)entity(app, f.target);
  ensure(app.capabilities.size() <= 64 && app.actions.size() <= 64, "schema_limit", "Too many capabilities or actions.");
  for (const auto& c : app.capabilities) {
    ensure(capability_id(c.id) && caps.insert(c.id).second && c.description.size() <= 2000, "invalid_capability", "Capability IDs must be versioned and unique.");
    fields_valid(c.inputs); fields_valid(c.outputs);
  }
  for (const auto& a : app.actions) {
    ensure(identifier(a.id) && actions.insert(a.id).second && !a.label.empty() && a.label.size() <= 160, "invalid_action", "Invalid action.");
    const auto& e = entity(app, a.entity); const auto& c = requirement(app, a.capability); roles_valid(a.roles, app.roles);
    for (const auto& r : a.roles) ensure(e.write_roles.contains(r), "invalid_role", "Action role requires write access to its entity.");
    for (const auto& f : c.inputs) if (f.required && !f.default_value) ensure(a.inputs.contains(f.id), "missing_binding", "Required capability input has no binding.");
    for (const auto& [to, from] : a.inputs) {
      const auto& target = field(c.inputs, to); const auto& source = field(e.fields, from);
      ensure(target.kind == source.kind && target.target == source.target, "binding_type", "Capability input type mismatch.");
    }
    for (const auto& [to, from] : a.outputs) {
      const auto& target = field(e.fields, to); const auto& source = field(c.outputs, from);
      ensure(target.kind == source.kind && target.target == source.target, "binding_type", "Capability output type mismatch.");
    }
  }
  for (const auto& p : app.pages) {
    ensure(identifier(p.id) && pages.insert(p.id).second && !p.title.empty() && p.title.size() <= 160 && !p.blocks.empty() && p.blocks.size() <= 32, "invalid_page", "Invalid page definition.");
    for (const auto& b : p.blocks) {
      ensure(b.title.size() <= 160, "invalid_block", "Block title too long.");
      if (b.kind == "files") { ensure(b.entity.empty() && b.action.empty() && b.fields.empty(), "invalid_block", "File library has no record binding."); continue; }
      if (b.kind == "action") { (void)action(app, b.action); ensure(b.entity.empty() && b.fields.empty(), "invalid_block", "Action block uses its registered action."); continue; }
      ensure(b.kind == "table" || b.kind == "form" || b.kind == "cards", "unknown_component", "UI component is not installed.");
      ensure(b.action.empty(), "invalid_block", "Record blocks cannot override actions."); const auto& e = entity(app, b.entity);
      ensure(b.fields.size() <= 64 && std::set<std::string>(b.fields.begin(), b.fields.end()).size() == b.fields.size(), "invalid_block", "Invalid displayed fields.");
      for (const auto& key : b.fields) (void)field(e.fields, key);
      if (b.kind == "form" && !b.fields.empty()) for (const auto& f : e.fields)
        if (f.required && !f.default_value) ensure(std::find(b.fields.begin(), b.fields.end(), f.id) != b.fields.end(), "incomplete_form", "Form omits a required field.");
    }
  }
}
inline void compatible_upgrade(const Spec& before, const Spec& after) {
  validate(after);
  for(const auto& role:before.roles) ensure(after.roles.contains(role),"migration_required","Removing roles requires explicit reassignment of existing memberships.");
  for (const auto& e : before.entities) {
    auto it = std::find_if(after.entities.begin(), after.entities.end(), [&](const Entity& n) { return n.id == e.id; });
    ensure(it != after.entities.end(), "migration_required", "Removing or renaming an entity requires an explicit data migration.");
    for (const auto& f : e.fields) {
      auto next = std::find_if(it->fields.begin(), it->fields.end(), [&](const Field& n) { return n.id == f.id; });
      ensure(next != it->fields.end(), "migration_required", "Removing or renaming a field requires an explicit data migration.");
      ensure(next->kind == f.kind && next->target == f.target && (!next->required || f.required)
             && next->max_length >= f.max_length && next->unique == f.unique, "migration_required", "Narrowing or changing field types/constraints requires a data migration.");
      for (const auto& option : f.options) ensure(std::find(next->options.begin(), next->options.end(), option) != next->options.end(), "migration_required", "Removing choices requires a data migration.");
      ensure(next->default_value == f.default_value, "migration_required", "Changing historical defaults requires an explicit backfill.");
    }
    for (const auto& f : it->fields) {
      const bool added = std::none_of(e.fields.begin(), e.fields.end(), [&](const Field& old) { return old.id == f.id; });
      if (added) ensure(!f.unique && (!f.required || f.default_value.has_value()), "migration_required", "New required fields need defaults; new uniqueness needs a migration.");
    }
  }
}
struct Capability {
  Requirement contract;
  // Only registered, bounded, deterministic handlers run in this local engine.
  // Remote or side-effecting handlers need the separate leased dispatch authority.
  std::function<Record(const Record&)> run;
};
class Registry {
  std::map<std::string, Capability> entries_;
public:
  void add(Capability entry) {
    ensure(capability_id(entry.contract.id) && static_cast<bool>(entry.run), "invalid_capability", "A capability requires a concrete implementation.");
    fields_valid(entry.contract.inputs); fields_valid(entry.contract.outputs);
    ensure(!entries_.contains(entry.contract.id), "duplicate_capability", "Installed versions are immutable.");
    const auto id = entry.contract.id; entries_.emplace(id, std::move(entry));
  }
  const Capability* find(const std::string& id) const { auto it = entries_.find(id); return it == entries_.end() ? nullptr : &it->second; }
};
inline bool same_contract(const std::vector<Field>& a, const std::vector<Field>& b) {
  if (a.size() != b.size()) return false;
  for (const auto& f : a) {
    auto it = std::find_if(b.begin(), b.end(), [&](const Field& g) { return f.id == g.id; });
    if (it == b.end() || f.kind != it->kind || f.required != it->required || f.max_length != it->max_length
        || f.options != it->options || f.target != it->target || f.default_value != it->default_value || f.unique != it->unique) return false;
  }
  return true;
}
inline std::string availability(const Requirement& required, const Registry& registry) {
  const auto* cap = registry.find(required.id);
  if (!cap) return "missing";
  return same_contract(required.inputs, cap->contract.inputs) && same_contract(required.outputs, cap->contract.outputs) ? "available" : "incompatible";
}
inline Record invoke(const Spec& app, const Action& a, const Record& record, const std::string& role, const Registry& registry) {
  authorize(a.roles, role); const auto& e = entity(app, a.entity); authorize(e.write_roles, role);
  const auto& need = requirement(app, a.capability);
  ensure(availability(need, registry) == "available", "capability_unavailable", "This action is blocked until a matching ability is installed. The application and data remain usable.");
  Record inputs; for (const auto& [to, from] : a.inputs) inputs[to] = record.at(from);
  const auto& cap = *registry.find(a.capability);
  const auto output = normalize(need.outputs, cap.run(normalize(need.inputs, inputs)));
  auto result = record; for (const auto& [to, from] : a.outputs) result[to] = output.at(from);
  return normalize(e.fields, result);
}
inline Registry builtins() {
  Registry registry;
  Field text; text.id = "text"; text.label = "Text"; text.required = true;
  Field words; words.id = "words"; words.label = "Words"; words.kind = Kind::integer; words.required = true;
  registry.add({{"core.text.word_count@1", "Count whitespace-delimited words without a model.", {text}, {words}}, [](const Record& input) {
    const auto& text_value = std::get<std::string>(input.at("text")); std::int64_t count = 0; bool inside = false;
    for (const char c : text_value) { const bool space = c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; if (!space && !inside) ++count; inside = !space; }
    return Record{{"words", std::to_string(count)}};
  }});
  return registry;
}
} // namespace runtime::application
