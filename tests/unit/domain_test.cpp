#include "runtime/domain.hpp"
#include <iostream>
#include <functional>
using namespace runtime;
int count = 0;
void test(const char* name, const std::function<void()>& fn) {
  try { fn(); ++count; std::cout << "PASS " << name << '\n'; }
  catch (const std::exception& e) { std::cerr << "FAIL " << name << ": " << e.what() << '\n'; std::exit(1); }
}
void check(bool value) { if (!value) throw std::runtime_error("expectation failed"); }
void rejects(std::string_view code, const std::function<void()>& fn) {
  try { fn(); } catch (const Error& e) { check(e.code == code); return; }
  throw std::runtime_error("expected rejection");
}
int main() {
  test("integer minor units", [] { check(minor_units("12345") == 12345); });
  test("large JS-unsafe integer preserved", [] { check(minor_units("9007199254740993") == 9007199254740993LL); });
  test("money rejects floats", [] { rejects("invalid_money", [] { minor_units("1.23"); }); });
  test("money rejects negative", [] { rejects("invalid_money", [] { minor_units("-1"); }); });
  test("money rejects overflow", [] { rejects("invalid_money", [] { minor_units("9223372036854775808"); }); });
  test("sum overflow", [] { rejects("money_overflow", [] { checked_add(INT64_MAX, 1); }); });
  test("leap date", [] { date("2024-02-29"); });
  test("non-leap date", [] { rejects("invalid_date", [] { date("2025-02-29"); }); });
  test("date trailing junk", [] { rejects("invalid_date", [] { date("2026-01-01X"); }); });
  test("CSV quoted comma and escaped quote", [] { auto r = parse_csv("a,b\r\n\"A, B\",\"He said \"\"yes\"\"\"\r\n"); check(r[1][0] == "A, B" && r[1][1] == "He said \"yes\""); });
  test("CSV quoted newline", [] { check(parse_csv("a\n\"x\ny\"")[1][0] == "x\ny"); });
  test("CSV unclosed quote", [] { rejects("invalid_csv", [] { parse_csv("a\n\"x"); }); });
  test("CSV wrong column count", [] { rejects("invalid_csv", [] { parse_csv("a,b\nx"); }); });
  test("CSV closed quote junk", [] { rejects("invalid_csv", [] { parse_csv("a\n\"b\"x"); }); });
  test("CSV column bound", [] { rejects("invalid_csv", [] { parse_csv(std::string(17, ',')); }); });
  test("CSV NUL rejected", [] { rejects("invalid_csv", [] { parse_csv(std::string("a\0b", 3)); }); });
  test("email header injection", [] { check(!email_valid("a@b.test\r\nBcc:x@y.test")); });
  const std::string header = "invoice_id,customer,email,amount_minor,currency,due_date,status\n";
  test("invoice import", [&] { check(import_invoices(header + "i1,A,a@b.test,100,USD,2026-01-01,open\n").size() == 1); });
  test("duplicate IDs", [&] { const std::string row = "i1,A,a@b.test,100,USD,2026-01-01,open\n"; rejects("duplicate_invoice", [&] { import_invoices(header + row + row); }); });
  test("unknown status", [&] { rejects("invalid_status", [&] { import_invoices(header + "i1,A,a@b.test,100,USD,2026-01-01,maybe\n"); }); });
  test("exclude paid and disputed", [&] { auto v = import_invoices(header + "i1,A,a@b.test,100,USD,2026-01-01,paid\ni2,B,b@b.test,200,USD,2026-01-01,disputed\n"); check(select_overdue(v,date("2026-02-01"),14).excluded == 2); });
  test("currency totals remain separate", [&] { auto v = import_invoices(header + "i1,A,a@b.test,100,USD,2026-01-01,open\ni2,B,b@b.test,200,JPY,2026-01-01,open\n"); auto r = select_overdue(v,date("2026-02-01"),14); check(r.totals.size() == 2 && r.totals.at("JPY") == 200); });
  test("strictly more than threshold", [&] { auto v = import_invoices(header + "i1,A,a@b.test,100,USD,2026-01-01,open\n"); check(select_overdue(v,date("2026-01-15"),14).eligible.empty()); });
  const std::map<std::string, Tool> registry{{"fixture.read@1",{false,NodeKind::read}},{"fixture.send@1",{true,NodeKind::execute}}};
  const std::vector<Node> valid{{"read",NodeKind::read,{},"fixture.read@1"},{"prepare",NodeKind::prepare,{"read"},""},{"approve",NodeKind::approval,{"prepare"},""},{"send",NodeKind::execute,{"approve"},"fixture.send@1"}};
  test("valid DAG", [&] { validate_workflow(valid, registry); });
  test("DAG cycle", [&] { auto n = valid; n[0].dependencies = {"send"}; rejects("workflow_cycle", [&] { validate_workflow(n,registry); }); });
  test("missing dependency", [&] { auto n = valid; n[1].dependencies = {"absent"}; rejects("missing_dependency", [&] { validate_workflow(n,registry); }); });
  test("unknown tool", [&] { auto n = valid; n[0].tool = "shell@1"; rejects("unknown_tool", [&] { validate_workflow(n,registry); }); });
  test("cannot disguise a write as read", [&] { auto n = valid; n[0].tool = "fixture.send@1"; rejects("tool_kind_mismatch", [&] { validate_workflow(n,registry); }); });
  test("disconnected approval is insufficient", [&] { auto n = valid; n[3].dependencies = {"prepare"}; rejects("approval_required", [&] { validate_workflow(n,registry); }); });
  test("approval before prepare is insufficient", [&] { auto n = valid; n[2].dependencies = {"read"}; rejects("approval_required", [&] { validate_workflow(n,registry); }); });
  test("duplicate node", [&] { auto n = valid; n.push_back(n[0]); rejects("invalid_node", [&] { validate_workflow(n,registry); }); });
  test("completed run is terminal", [] { check(!transition_allowed(State::completed,State::executing)); });
  test("ambiguous write needs reconciliation", [] { check(transition_allowed(State::executing,State::needs_reconciliation)); check(!transition_allowed(State::needs_reconciliation,State::executing)); });
  Approval approval{"workspace-a","hash-a",2,1000};
  test("valid exact approval", [&] { validate_approval(approval,"workspace-a","owner","hash-a",2,999,1); });
  test("cross-workspace approval", [&] { rejects("not_found",[&] { validate_approval(approval,"workspace-b","owner","hash-a",2,999,1); }); });
  test("viewer cannot approve", [&] { rejects("forbidden",[&] { validate_approval(approval,"workspace-a","viewer","hash-a",2,999,1); }); });
  test("stale version", [&] { rejects("stale_approval",[&] { validate_approval(approval,"workspace-a","owner","hash-a",1,999,1); }); });
  test("stale payload", [&] { rejects("stale_approval",[&] { validate_approval(approval,"workspace-a","owner","hash-b",2,999,1); }); });
  test("expiry boundary", [&] { rejects("expired_approval",[&] { validate_approval(approval,"workspace-a","owner","hash-a",2,1000,1); }); });
  test("batch cap", [&] { rejects("action_limit",[&] { validate_approval(approval,"workspace-a","owner","hash-a",2,999,11); }); });
  test("deterministic bypass", [] { check(route(true,false,false,"",0) == "deterministic"); });
  test("router failure is conservative", [] { check(route(false,false,false,"",0) == "planner"); });
  test("router cannot bypass evaluation gate", [] { check(route(false,false,true,"routine",1) == "planner"); });
  std::cout << count << " tests passed\n";
}
