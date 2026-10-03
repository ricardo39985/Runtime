#pragma once
#include <boost/json.hpp>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace runtime::apps {
namespace j = boost::json;
struct Fault : std::runtime_error {
  std::string code;
  Fault(std::string code_, const std::string& message) : std::runtime_error(message), code(std::move(code_)) {}
};
void need(bool condition, std::string_view code, std::string_view message);
j::value parse(std::string_view text, std::size_t limit = 256 * 1024);
std::string text(const j::value& value);
std::string str(const j::object& object, std::string_view key);
std::int64_t integer(const j::value& value);
bool identifier(std::string_view value);
void keys(const j::object& object, std::initializer_list<std::string_view> allowed);

struct Capability {
  std::string name;
  std::int64_t version = 1;
  std::string contract;
  bool available = false;
  // Provider adapters must register trusted implementations, never application-supplied code.
  std::function<j::value(const j::value&)> invoke;
};
class Registry {
  std::map<std::pair<std::string,std::int64_t>,Capability> entries_;
public:
  void install(Capability capability);
  j::object resolve(const j::object& requirement) const;
  j::value invoke(const j::object& requirement, const j::value& input) const;
};
struct Application {
  j::object spec;
  explicit Application(const j::value& input);
  const j::object& entity(std::string_view key) const;
  const j::object& action(std::string_view key) const;
  void authorize(std::string_view entity_key, std::string_view role, bool write) const;
  j::object validate_record(std::string_view entity_key, const j::value& input) const;
  void compatible_with(const Application& previous) const;
  j::object describe(const Registry& registry, std::string_view role) const;
};
Registry builtins();
} // namespace runtime::apps
