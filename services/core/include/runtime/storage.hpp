#pragma once
#include "runtime/application.hpp"
#include <cstdint>
#include <string>
#include <string_view>
namespace runtime::apps {
inline constexpr std::size_t max_asset_bytes = 8 * 1024 * 1024;
struct StoredObject {
  std::string sha256, codec, bytes;
  std::size_t original_size=0;
};
std::string hash(std::string_view value);
std::string hex(std::string_view value);
std::string unhex(std::string_view value);
std::string new_id();
StoredObject encode(std::string_view original);
std::string decode(const StoredObject& object);
std::string safe_filename(std::string_view name);
} // namespace runtime::apps
