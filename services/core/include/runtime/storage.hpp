#pragma once
#include "runtime/application.hpp"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <zstd.h>
#include <array>
#include <cerrno>
#include <filesystem>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace runtime::storage {
using application::ensure;
inline constexpr std::size_t max_object_bytes = 8 * 1024 * 1024;
inline constexpr std::size_t header_bytes = 8 + 1 + 8 + 8 + 32;
inline std::array<unsigned char, 32> checksum(std::string_view input) {
  std::array<unsigned char, 32> result{}; unsigned int count = 0;
  ensure(EVP_Digest(input.data(), input.size(), result.data(), &count, EVP_sha256(), nullptr) == 1 && count == 32,
         "storage_crypto", "Content hashing failed."); return result;
}
inline std::string hex_hash(const std::array<unsigned char, 32>& value) {
  constexpr char digits[] = "0123456789abcdef"; std::string result;
  for (auto c : value) { result += digits[c >> 4]; result += digits[c & 15]; } return result;
}
inline std::string object_id() {
  std::array<unsigned char, 16> bytes{};
  ensure(RAND_bytes(bytes.data(), bytes.size()) == 1, "storage_crypto", "Secure random ID unavailable.");
  std::string result; constexpr char chars[] = "0123456789abcdef";
  for (const auto c : bytes) { result += chars[c >> 4]; result += chars[c & 15]; } return result;
}
inline bool valid_key(std::string_view key) {
  return key.size() == 32 && key.find_first_not_of("0123456789abcdef") == std::string_view::npos;
}
inline void put_number(std::string& out, std::uint64_t n) { for (int i = 7; i >= 0; --i) out += static_cast<char>((n >> (i * 8)) & 255); }
inline std::uint64_t get_number(std::string_view data, std::size_t offset) {
  ensure(offset + 8 <= data.size(), "corrupt_object", "Truncated object header.");
  std::uint64_t out = 0; for (std::size_t i = 0; i < 8; ++i) out = (out << 8) | static_cast<unsigned char>(data[offset + i]); return out;
}
struct Encoded { std::string bytes, codec, sha256; std::size_t original_size, stored_size; };
inline Encoded encode(std::string_view input) {
  ensure(input.size() <= max_object_bytes, "object_limit", "Upload exceeds the 8 MiB limit.");
  std::string compressed(ZSTD_compressBound(input.size()), '\0');
  const auto count = ZSTD_compress(compressed.data(), compressed.size(), input.data(), input.size(), 3);
  ensure(!ZSTD_isError(count), "compression_failed", "Lossless compression failed."); compressed.resize(count);
  const bool worthwhile = compressed.size() + 32 < input.size();
  const auto digest = checksum(input);
  std::string envelope = "RTOBJ001"; envelope += worthwhile ? '\1' : '\0';
  put_number(envelope, input.size()); put_number(envelope, worthwhile ? compressed.size() : input.size());
  envelope.append(reinterpret_cast<const char*>(digest.data()), digest.size());
  envelope.append(worthwhile ? std::string_view(compressed) : input);
  return {envelope, worthwhile ? "zstd" : "identity", hex_hash(digest), input.size(), envelope.size()};
}
inline std::string decode(std::string_view stored) {
  ensure(stored.size() >= header_bytes && stored.size() <= max_object_bytes + header_bytes && stored.substr(0, 8) == "RTOBJ001", "corrupt_object", "Invalid object envelope.");
  const auto original = get_number(stored, 9), payload_size = get_number(stored, 17);
  ensure(original <= max_object_bytes && payload_size <= max_object_bytes && payload_size == stored.size() - header_bytes,
         "object_limit", "Invalid or oversized decompression request.");
  const auto payload = stored.substr(header_bytes); std::string out;
  if (stored[8] == '\0') {
    ensure(original == payload.size(), "corrupt_object", "Object size mismatch."); out = payload;
  } else {
    ensure(stored[8] == '\1', "unknown_codec", "Storage codec is not installed.");
    ensure(ZSTD_getFrameContentSize(payload.data(), payload.size()) == original, "corrupt_object", "Compressed size declaration mismatch.");
    out.resize(original);
    const auto count = ZSTD_decompress(out.data(), out.size(), payload.data(), payload.size());
    ensure(!ZSTD_isError(count) && count == original, "corrupt_object", "Compressed object failed validation.");
  }
  const auto actual = checksum(out);
  ensure(std::equal(actual.begin(), actual.end(), reinterpret_cast<const unsigned char*>(stored.data() + 25)), "corrupt_object", "Content checksum mismatch.");
  return out;
}
class FileDescriptor {
  int fd_;
public:
  explicit FileDescriptor(int fd) : fd_(fd) { ensure(fd_ >= 0, "storage_io", "Object storage operation failed."); }
  ~FileDescriptor() { if (fd_ >= 0) ::close(fd_); }
  FileDescriptor(const FileDescriptor&) = delete; FileDescriptor& operator=(const FileDescriptor&) = delete;
  int get() const { return fd_; }
};
class ObjectStore {
public:
  virtual ~ObjectStore() = default;
  virtual void put(const std::string& key, std::string_view encoded) = 0;
  virtual std::string get(const std::string& key) const = 0;
};
// The server selects opaque keys after database authorization. No filename, tenant
// name, user URL or relative path is ever used as a storage path. Objects cannot be
// overwritten. Database metadata owns visibility and lifecycle; bytes alone grant no access.
class LocalObjectStore final : public ObjectStore {
  FileDescriptor root_;
  static int root_directory(const std::filesystem::path& path) {
    std::filesystem::create_directories(path);
    const int fd = ::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    ensure(fd >= 0, "storage_io", "Cannot open the configured private object root.");
    if (::fchmod(fd, 0700) != 0) { ::close(fd); throw application::Invalid("storage_io", "Cannot restrict object directory permissions."); }
    return fd;
  }
public:
  explicit LocalObjectStore(const std::filesystem::path& root) : root_(root_directory(root)) {}
  void put(const std::string& key, std::string_view data) override {
    ensure(valid_key(key) && data.size() <= max_object_bytes + header_bytes, "invalid_object", "Invalid storage key or size.");
    const std::string temp = ".pending-" + object_id();
    try {
      FileDescriptor file(::openat(root_.get(), temp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600));
      std::size_t offset = 0;
      while (offset < data.size()) {
        const auto n = ::write(file.get(), data.data() + offset, data.size() - offset);
        if (n < 0 && errno == EINTR) continue;
        ensure(n > 0, "storage_io", "Object write failed."); offset += static_cast<std::size_t>(n);
      }
      ensure(::fsync(file.get()) == 0, "storage_io", "Object flush failed.");
      // linkat is atomic and fails if key already exists; never replace an old asset.
      ensure(::linkat(root_.get(), temp.c_str(), root_.get(), key.c_str(), 0) == 0, "object_exists", "Object already exists or publication failed.");
      ensure(::unlinkat(root_.get(), temp.c_str(), 0) == 0 && ::fsync(root_.get()) == 0, "storage_io", "Object directory flush failed.");
    } catch (...) { ::unlinkat(root_.get(), temp.c_str(), 0); throw; }
  }
  std::string get(const std::string& key) const override {
    ensure(valid_key(key), "invalid_object", "Invalid storage key.");
    FileDescriptor file(::openat(root_.get(), key.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
    struct stat info{};
    ensure(::fstat(file.get(), &info) == 0 && S_ISREG(info.st_mode) && info.st_size >= 0
           && static_cast<std::uint64_t>(info.st_size) <= max_object_bytes + header_bytes, "corrupt_object", "Invalid stored object.");
    std::string bytes(static_cast<std::size_t>(info.st_size), '\0'); std::size_t offset = 0;
    while (offset < bytes.size()) {
      const auto count = ::read(file.get(), bytes.data() + offset, bytes.size() - offset);
      if (count < 0 && errno == EINTR) continue;
      ensure(count > 0, "storage_io", "Stored object was truncated."); offset += static_cast<std::size_t>(count);
    }
    return bytes;
  }
};
} // namespace runtime::storage
