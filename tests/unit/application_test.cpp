#include "runtime/application.hpp"
#include <iostream>
using namespace runtime::application;
int passed = 0;
void test(const char* name, const std::function<void()>& fn) {
  try { fn(); ++passed; std::cout << "PASS " << name << '\n'; }
  catch (const std::exception& e) { std::cerr << "FAIL " << name << ": " << e.what() << '\n'; std::exit(1); }
}
void check(bool ok) { if (!ok) throw std::runtime_error("Assertion failed"); }
void fails(std::string_view code, const std::function<void()>& fn) {
  try { fn(); } catch (const Invalid& e) { check(e.code == code); return; }
  throw std::runtime_error("Expected rejection");
}
Spec sample() {
  Field title; title.id="title"; title.label="Title"; title.required=true;
  Field body; body.id="body"; body.label="Body"; body.required=true;
  Field count; count.id="word_count"; count.label="Word count"; count.kind=Kind::integer;
  auto registry=builtins(); auto need=registry.find("core.text.word_count@1")->contract;
  Spec s; s.name="An arbitrary application"; s.roles={"owner","reader"};
  s.entities={{"documents","Documents",{title,body,count},{"owner","reader"},{"owner"}}};
  s.capabilities={need}; s.actions={{"count_words","Count words","documents",need.id,{{"text","body"}},{{"word_count","words"}},{"owner"}}};
  s.pages={{"home","Documents",{{"form","New document","documents","",{}},{"table","All documents","documents","",{}},{"files","Library","","",{}},{"action","","","count_words",{}}}}};
  return s;
}
int main() {
  test("generic app compiles",[]{validate(sample());});
  test("app names do not choose code paths",[]{for(int i=0;i<50;++i){auto s=sample();s.name="Application "+std::to_string(i);validate(s);}});
  test("missing paint ability does not reject app",[]{auto s=sample();s.capabilities[0].id="creative.paint@1";s.actions[0].capability="creative.paint@1";validate(s);check(availability(s.capabilities[0],builtins())=="missing");});
  test("missing ability execution blocked",[]{auto s=sample();s.capabilities[0].id="creative.paint@1";s.actions[0].capability="creative.paint@1";auto row=normalize(s.entities[0].fields,{{"title",std::string("x")},{"body",std::string("y")}});fails("capability_unavailable",[&]{invoke(s,s.actions[0],row,"owner",builtins());});});
  test("real installed ability executes",[]{auto s=sample();auto row=normalize(s.entities[0].fields,{{"title",std::string("x")},{"body",std::string("one two\nthree")}});auto out=invoke(s,s.actions[0],row,"owner",builtins());check(std::get<std::string>(out.at("word_count"))=="3");});
  test("adding adapter resolves missing slot without app change",[]{auto s=sample();s.capabilities[0].id="custom.count@1";s.actions[0].capability="custom.count@1";auto r=builtins();check(availability(s.capabilities[0],r)=="missing");r.add({s.capabilities[0],[](const Record&){return Record{{"words",std::string("4")}};}});check(availability(s.capabilities[0],r)=="available");});
  test("installed version cannot be overwritten",[]{auto r=builtins();auto copy=*r.find("core.text.word_count@1");fails("duplicate_capability",[&]{r.add(copy);});});
  test("manifest only cannot masquerade as an ability",[]{auto s=sample();Registry r;fails("invalid_capability",[&]{r.add({s.capabilities[0],{}});});});
  test("incompatible provider contract remains blocked",[]{auto s=sample();s.capabilities[0].inputs[0].max_length=16;check(availability(s.capabilities[0],builtins())=="incompatible");});
  test("role cannot invoke write action",[]{auto s=sample();auto row=normalize(s.entities[0].fields,{{"title",std::string("x")},{"body",std::string("y")}});fails("forbidden",[&]{invoke(s,s.actions[0],row,"reader",builtins());});});
  test("unknown field cannot be stored",[]{auto s=sample();fails("unknown_field",[&]{normalize(s.entities[0].fields,{{"invented",std::string("x")}});});});
  test("required fields enforced",[]{auto s=sample();fails("required_field",[&]{normalize(s.entities[0].fields,{});});});
  test("large integer remains exact",[]{check(integer("9007199254740993")==9007199254740993LL);});
  test("integer normalization rejects alternative encoding",[]{fails("invalid_integer",[]{integer("01");});});
  test("integer rejects decimal",[]{fails("invalid_integer",[]{integer("1.1");});});
  test("integer rejects overflow",[]{fails("invalid_integer",[]{integer("9223372036854775808");});});
  test("invalid leap day rejected",[]{fails("invalid_date",[]{validate_date("2025-02-29");});});
  test("valid leap day accepted",[]{validate_date("2024-02-29");});
  test("safe identifier",[]{check(!identifier("../../etc"));check(!identifier("constructor"));check(!identifier("x;drop table"));});
  test("version required for capabilities",[]{check(!capability_id("creative.paint"));});
  test("duplicate entity IDs rejected",[]{auto s=sample();s.entities.push_back(s.entities[0]);fails("invalid_entity",[&]{validate(s);});});
  test("unknown page entity rejected",[]{auto s=sample();s.pages[0].blocks[0].entity="absent";fails("unknown_entity",[&]{validate(s);});});
  test("unknown UI widget fails honestly",[]{auto s=sample();s.pages[0].blocks[0].kind="arbitrary_javascript";fails("unknown_component",[&]{validate(s);});});
  test("forms cannot omit required fields",[]{auto s=sample();s.pages[0].blocks[0].fields={"title"};fails("incomplete_form",[&]{validate(s);});});
  test("unknown application role rejected",[]{auto s=sample();s.entities[0].write_roles.insert("intruder");fails("unknown_role",[&]{validate(s);});});
  test("action bindings type checked",[]{auto s=sample();s.actions[0].inputs["text"]="word_count";fails("binding_type",[&]{validate(s);});});
  test("required action input must be bound",[]{auto s=sample();s.actions[0].inputs.clear();fails("missing_binding",[&]{validate(s);});});
  test("add optional field preserves schema compatibility",[]{auto a=sample(),b=a;Field f;f.id="notes";f.label="Notes";b.entities[0].fields.push_back(f);compatible_upgrade(a,b);});
  test("new required field needs default",[]{auto a=sample(),b=a;Field f;f.id="notes";f.label="Notes";f.required=true;b.entities[0].fields.push_back(f);fails("migration_required",[&]{compatible_upgrade(a,b);});});
  test("new required field with default reads old records",[]{auto a=sample(),b=a;Field f;f.id="notes";f.label="Notes";f.required=true;f.default_value=std::string("pending");b.entities[0].fields.push_back(f);compatible_upgrade(a,b);auto row=normalize(b.entities[0].fields,{{"title",std::string("x")},{"body",std::string("y")}});check(std::get<std::string>(row.at("notes"))=="pending");});
  test("dropping field rejected before data damage",[]{auto a=sample(),b=a;b.entities[0].fields.pop_back();b.actions.clear();b.pages[0].blocks.pop_back();fails("migration_required",[&]{compatible_upgrade(a,b);});});
  test("narrower max length requires migration",[]{auto a=sample(),b=a;b.entities[0].fields[0].max_length=8;fails("migration_required",[&]{compatible_upgrade(a,b);});});
  test("renaming entity is not silent deletion",[]{auto a=sample(),b=a;auto copy=b.entities[0];copy.id="renamed";b.entities={copy};for(auto& x:b.pages[0].blocks)if(!x.entity.empty())x.entity="renamed";b.actions[0].entity="renamed";fails("migration_required",[&]{compatible_upgrade(a,b);});});
  test("reference target must exist",[]{auto s=sample();Field f;f.id="parent";f.label="Parent";f.kind=Kind::reference;f.target="missing";s.entities[0].fields.push_back(f);fails("unknown_entity",[&]{validate(s);});});
  test("file fields use opaque IDs",[]{Field f;f.id="attachment";f.label="Attachment";f.kind=Kind::file;fails("invalid_reference",[&]{validate_value(f,std::string("../../secret"));});});
  test("narrowed enum upgrade rejected",[]{auto a=sample();Field f;f.id="status";f.label="Status";f.kind=Kind::choice;f.options={"open","closed"};a.entities[0].fields.push_back(f);auto b=a;b.entities[0].fields.back().options={"open"};fails("migration_required",[&]{compatible_upgrade(a,b);});});
  test("capability output validated before storage",[]{auto s=sample();Registry r;r.add({s.capabilities[0],[](const Record&){return Record{{"words",std::string("not an integer")}};}});auto row=normalize(s.entities[0].fields,{{"title",std::string("x")},{"body",std::string("y")}});fails("invalid_integer",[&]{invoke(s,s.actions[0],row,"owner",r);});});
  std::cout<<passed<<" application tests passed\n";
}
