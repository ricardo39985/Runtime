#pragma once
#include "runtime/domain.hpp"
#include <libpq-fe.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

namespace runtime {
inline std::string hex(const unsigned char* bytes, std::size_t size) {
  static constexpr char digits[] = "0123456789abcdef";
  std::string value; value.reserve(size * 2);
  for (std::size_t i = 0; i < size; ++i) { value += digits[bytes[i] >> 4]; value += digits[bytes[i] & 15]; }
  return value;
}
inline std::string random_id() {
  unsigned char bytes[16];
  require(RAND_bytes(bytes, sizeof bytes) == 1, "crypto_error", "Secure randomness unavailable.");
  return hex(bytes, sizeof bytes);
}
inline std::string sha256(std::string_view input) {
  unsigned char result[EVP_MAX_MD_SIZE]; unsigned int size = 0;
  require(EVP_Digest(input.data(), input.size(), result, &size, EVP_sha256(), nullptr) == 1,
          "crypto_error", "Digest unavailable.");
  return hex(result, size);
}
inline bool secure_equal(std::string_view left, std::string_view right) {
  return left.size() == right.size() && CRYPTO_memcmp(left.data(), right.data(), left.size()) == 0;
}
class Pool {
  std::mutex mutex_; std::condition_variable changed_;
  std::deque<std::function<void()>> tasks_; std::vector<std::thread> threads_;
  bool stopping_ = false;
public:
  explicit Pool(std::size_t count = 4) {
    for (std::size_t i = 0; i < count; ++i) threads_.emplace_back([this] {
      for (;;) {
        std::function<void()> task;
        { std::unique_lock lock(mutex_); changed_.wait(lock,[&] { return stopping_ || !tasks_.empty(); });
          if (tasks_.empty() && stopping_) return;
          task = std::move(tasks_.front()); tasks_.pop_front(); }
        task();
      }
    });
  }
  bool submit(std::function<void()> task) {
    { std::lock_guard lock(mutex_); if (stopping_ || tasks_.size() >= 64) return false; tasks_.push_back(std::move(task)); }
    changed_.notify_one(); return true;
  }
  ~Pool() {
    { std::lock_guard lock(mutex_); stopping_ = true; }
    changed_.notify_all(); for (auto& thread : threads_) thread.join();
  }
  Pool(const Pool&) = delete; Pool& operator=(const Pool&) = delete;
};
struct Result {
  std::unique_ptr<PGresult, decltype(&PQclear)> value;
  explicit Result(PGresult* raw) : value(raw,PQclear) {
    require(raw != nullptr && (PQresultStatus(raw) == PGRES_TUPLES_OK || PQresultStatus(raw) == PGRES_COMMAND_OK),
            "database_error", "Database operation failed.");
  }
  int size() const { return PQntuples(value.get()); }
  std::string get(int row, int column) const { return PQgetvalue(value.get(),row,column); }
};
class Database {
  std::unique_ptr<PGconn, decltype(&PQfinish)> connection_;
  bool transaction_ = false;
public:
  static PGconn* connect(const std::string& url) {
    const char* keys[] = {"dbname","connect_timeout",nullptr};
    const char* values[] = {url.c_str(),"5",nullptr};
    return PQconnectdbParams(keys,values,1);
  }
  explicit Database(const std::string& url) : connection_(connect(url),PQfinish) {
    require(connection_ && PQstatus(connection_.get()) == CONNECTION_OK,"database_unavailable","Database unavailable.");
  }
  Result query(const std::string& sql, const std::vector<std::string>& params = {}) {
    std::vector<const char*> values; for (const auto& parameter : params) values.push_back(parameter.c_str());
    return Result(PQexecParams(connection_.get(),sql.c_str(),static_cast<int>(values.size()),nullptr,values.data(),nullptr,nullptr,0));
  }
  void begin(const std::string& workspace) {
    query("BEGIN"); transaction_ = true;
    query("SELECT set_config('app.workspace_id',$1,true), set_config('statement_timeout','5000',true), set_config('lock_timeout','2000',true)",{workspace});
  }
  void commit() { query("COMMIT"); transaction_ = false; }
  ~Database() { if (transaction_) { auto* result = PQexec(connection_.get(),"ROLLBACK"); if (result) PQclear(result); } }
};
} // namespace runtime
