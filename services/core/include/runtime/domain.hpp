#pragma once
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace runtime {
struct Error : std::runtime_error {
  std::string code;
  Error(std::string c, std::string message) : std::runtime_error(std::move(message)), code(std::move(c)) {}
};
inline void require(bool condition, std::string_view code, std::string_view message) {
  if (!condition) throw Error(std::string(code), std::string(message));
}
inline std::int64_t minor_units(std::string_view value) {
  require(!value.empty() && value.size() <= 19, "invalid_money", "Use nonnegative integer minor units.");
  require(value.find_first_not_of("0123456789") == std::string_view::npos, "invalid_money", "Money must contain digits only.");
  std::int64_t result = 0;
  auto [end, ec] = std::from_chars(value.data(), value.data() + value.size(), result);
  require(ec == std::errc{} && end == value.data() + value.size(), "invalid_money", "Amount is outside int64 range.");
  return result;
}
inline std::int64_t checked_add(std::int64_t left, std::int64_t right) {
  require(left >= 0 && right >= 0 && left <= std::numeric_limits<std::int64_t>::max() - right,
          "money_overflow", "Total exceeds the supported range.");
  return left + right;
}
inline std::chrono::sys_days date(std::string_view text) {
  require(text.size() == 10 && text[4] == '-' && text[7] == '-', "invalid_date", "Use YYYY-MM-DD.");
  auto part = [&](std::size_t begin, std::size_t length) {
    auto value = text.substr(begin, length);
    require(value.find_first_not_of("0123456789") == std::string_view::npos, "invalid_date", "Invalid date digits.");
    unsigned result = 0;
    auto [end, ec] = std::from_chars(value.data(), value.data() + value.size(), result);
    require(ec == std::errc{} && end == value.data() + value.size(), "invalid_date", "Invalid date.");
    return result;
  };
  std::chrono::year_month_day ymd{std::chrono::year(static_cast<int>(part(0, 4))),
                                 std::chrono::month(part(5, 2)), std::chrono::day(part(8, 2))};
  require(ymd.ok() && int(ymd.year()) >= 1900, "invalid_date", "Invalid calendar date.");
  return std::chrono::sys_days{ymd};
}
inline std::vector<std::vector<std::string>> parse_csv(std::string_view input) {
  require(!input.empty() && input.size() <= 256 * 1024, "invalid_csv", "CSV must be between 1 byte and 256 KiB.");
  std::vector<std::vector<std::string>> rows;
  std::vector<std::string> row;
  std::string field;
  bool quoted = false, closed = false;
  auto finish_field = [&] {
    require(field.size() <= 4096 && row.size() < 16, "invalid_csv", "CSV cell or column limit exceeded.");
    row.push_back(std::move(field)); field.clear(); closed = false;
  };
  auto finish_row = [&] {
    finish_field();
    require(rows.size() < 501, "invalid_csv", "At most 500 records are supported.");
    rows.push_back(std::move(row)); row.clear();
  };
  for (std::size_t i = 0; i < input.size(); ++i) {
    const char c = input[i];
    require(c != '\0', "invalid_csv", "NUL bytes are not permitted.");
    if (quoted) {
      if (c == '"') {
        if (i + 1 < input.size() && input[i + 1] == '"') { field += '"'; ++i; }
        else { quoted = false; closed = true; }
      } else field += c;
    } else if (c == ',') finish_field();
    else if (c == '\n' || c == '\r') {
      finish_row();
      if (c == '\r' && i + 1 < input.size() && input[i + 1] == '\n') ++i;
    } else if (c == '"') {
      require(field.empty() && !closed, "invalid_csv", "Unexpected quote."); quoted = true;
    } else {
      require(!closed, "invalid_csv", "Unexpected data after closing quote."); field += c;
    }
    require(field.size() <= 4096, "invalid_csv", "CSV cell limit exceeded.");
  }
  require(!quoted, "invalid_csv", "Unclosed quoted field.");
  if (!field.empty() || !row.empty() || closed) finish_row();
  require(!rows.empty(), "invalid_csv", "CSV has no rows.");
  for (const auto& r : rows) require(r.size() == rows.front().size(), "invalid_csv", "CSV column counts differ.");
  return rows;
}
struct Invoice {
  std::string id, customer, email, currency, due_date, status;
  std::int64_t amount = 0;
};
inline bool email_valid(std::string_view email) {
  auto at = email.find('@');
  return email.size() <= 254 && at != std::string_view::npos && at > 0 && at + 1 < email.size()
    && email.find('@', at + 1) == std::string_view::npos
    && email.find_first_of("\r\n\t ,;<>\"\\") == std::string_view::npos;
}
inline std::vector<Invoice> import_invoices(std::string_view input) {
  auto rows = parse_csv(input);
  const std::vector<std::string> header{"invoice_id","customer","email","amount_minor","currency","due_date","status"};
  require(rows.front() == header, "invalid_csv_header", "Expected invoice_id,customer,email,amount_minor,currency,due_date,status.");
  std::set<std::string> ids;
  std::vector<Invoice> result;
  for (std::size_t i = 1; i < rows.size(); ++i) {
    const auto& r = rows[i];
    require(!r[0].empty() && r[0].size() <= 128 && ids.insert(r[0]).second, "duplicate_invoice", "Invoice IDs must be unique and nonempty.");
    require(!r[1].empty() && r[1].size() <= 200, "invalid_customer", "Customer name required (max 200 characters).");
    require(email_valid(r[2]), "invalid_email", "An unambiguous email is required for every row.");
    require(r[4].size() == 3 && r[4].find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ") == std::string::npos,
            "invalid_currency", "Use a three-letter uppercase currency identifier.");
    date(r[5]);
    require(r[6] == "open" || r[6] == "paid" || r[6] == "void" || r[6] == "disputed" || r[6] == "suppressed",
            "invalid_status", "Unknown invoice status.");
    result.push_back({r[0],r[1],r[2],r[4],r[5],r[6],minor_units(r[3])});
  }
  return result;
}
struct Receivables {
  std::vector<Invoice> eligible;
  std::map<std::string, std::int64_t> totals;
  std::size_t excluded = 0;
};
inline Receivables select_overdue(const std::vector<Invoice>& invoices, std::chrono::sys_days today, int days) {
  require(days >= 0 && days <= 3650, "invalid_filter", "Overdue threshold must be 0–3650 days.");
  Receivables result;
  for (const auto& invoice : invoices) {
    if (invoice.status != "open" || invoice.amount == 0 || (today - date(invoice.due_date)).count() <= days) {
      ++result.excluded; continue;
    }
    result.totals[invoice.currency] = checked_add(result.totals[invoice.currency], invoice.amount);
    result.eligible.push_back(invoice);
  }
  return result;
}
enum class NodeKind { read, transform, render, prepare, approval, execute, verify };
struct Node { std::string id; NodeKind kind; std::vector<std::string> dependencies; std::string tool; };
struct Tool { bool external_write; NodeKind kind; };
inline void validate_workflow(const std::vector<Node>& nodes, const std::map<std::string, Tool>& registry) {
  require(!nodes.empty() && nodes.size() <= 32, "invalid_workflow", "A workflow needs 1–32 nodes.");
  std::map<std::string, const Node*> index;
  for (const auto& node : nodes) {
    require(!node.id.empty() && node.id.size() <= 64 && index.emplace(node.id, &node).second,
            "invalid_node", "Node IDs must be unique.");
    require(node.dependencies.size() <= 32, "invalid_node", "Too many dependencies.");
    require(std::set<std::string>(node.dependencies.begin(), node.dependencies.end()).size() == node.dependencies.size(),
            "invalid_node", "Duplicate dependency.");
    if (!node.tool.empty()) {
      auto it = registry.find(node.tool);
      require(it != registry.end(), "unknown_tool", "Tool version is not registered.");
      require(it->second.kind == node.kind && (!it->second.external_write || node.kind == NodeKind::execute),
              "tool_kind_mismatch", "Tool effect class cannot be changed by a plan.");
    }
    if (node.kind == NodeKind::execute || node.kind == NodeKind::read)
      require(!node.tool.empty(), "missing_tool", "Executable capability required.");
  }
  std::map<std::string, int> colors;
  std::map<std::string, std::set<std::string>> ancestors;
  std::function<void(const std::string&)> visit = [&](const std::string& id) {
    require(index.contains(id), "missing_dependency", "Unknown dependency.");
    require(colors[id] != 1, "workflow_cycle", "Workflow contains a cycle.");
    if (colors[id] == 2) return;
    colors[id] = 1;
    for (const auto& parent : index.at(id)->dependencies) {
      visit(parent); ancestors[id].insert(parent);
      ancestors[id].insert(ancestors[parent].begin(), ancestors[parent].end());
    }
    colors[id] = 2;
  };
  for (const auto& node : nodes) visit(node.id);
  for (const auto& node : nodes) if (node.kind == NodeKind::execute) {
    bool guarded = false;
    for (const auto& parent : ancestors[node.id]) if (index.at(parent)->kind == NodeKind::approval) {
      for (const auto& before : ancestors[parent]) if (index.at(before)->kind == NodeKind::prepare) guarded = true;
    }
    require(guarded, "approval_required", "Execution must depend on approval of a prepared action.");
  }
}
enum class State { queued, preparing, awaiting_approval, executing, completed, failed, cancelled, needs_reconciliation };
inline bool transition_allowed(State from, State to) {
  switch (from) {
    case State::queued: return to == State::preparing || to == State::cancelled || to == State::failed;
    case State::preparing: return to == State::awaiting_approval || to == State::completed || to == State::failed || to == State::cancelled;
    case State::awaiting_approval: return to == State::executing || to == State::cancelled;
    case State::executing: return to == State::completed || to == State::failed || to == State::needs_reconciliation;
    default: return false;
  }
}
struct Approval {
  std::string workspace, payload_hash;
  std::uint64_t version;
  std::int64_t expires_at;
};
inline void validate_approval(const Approval& approval, std::string_view workspace, std::string_view role,
                              std::string_view payload_hash, std::uint64_t version, std::int64_t now, std::size_t count) {
  require(approval.workspace == workspace, "not_found", "Proposal not found.");
  require(role == "owner" || role == "operator", "forbidden", "Approval permission required.");
  require(approval.version == version && approval.payload_hash == payload_hash, "stale_approval", "The proposal has changed. Review it again.");
  require(now < approval.expires_at, "expired_approval", "Approval expired. Prepare a fresh proposal.");
  require(count > 0 && count <= 10, "action_limit", "Approve 1–10 actions at a time.");
}
inline std::string route(bool deterministic, bool saved, bool router_ok, std::string_view choice, double confidence) {
  if (deterministic || saved) return "deterministic";
  // Cost-saving routing is deliberately disabled until the evaluation gate passes.
  (void)router_ok; (void)choice; (void)confidence;
  return "planner";
}
} // namespace runtime
