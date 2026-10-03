#include "runtime/storage.hpp"
#include <iostream>
using namespace runtime::storage;
int passed=0;
void check(bool yes){if(!yes)throw std::runtime_error("Assertion failed");}
void test(const char* name,const std::function<void()>& fn){try{fn();++passed;std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& e){std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n';std::exit(1);}}
void rejects(const std::function<void()>& fn){try{fn();}catch(const runtime::application::Invalid&){return;}throw std::runtime_error("Expected rejection");}
int main(){
  test("lossless compressible roundtrip",[]{auto src=std::string(100000,'A');auto packed=encode(src);check(packed.codec=="zstd"&&packed.stored_size<packed.original_size&&decode(packed.bytes)==src);});
  test("empty file roundtrip",[]{auto p=encode("");check(p.codec=="identity"&&decode(p.bytes).empty());});
  test("binary content including NUL roundtrips",[]{std::string s;for(int i=0;i<1024;++i)s+=static_cast<char>(i%256);check(decode(encode(s).bytes)==s);});
  test("incompressible data not inflated",[]{std::string s(4096,'x');check(RAND_bytes(reinterpret_cast<unsigned char*>(s.data()),s.size())==1);auto p=encode(s);check(p.codec=="identity"&&p.stored_size==s.size()+header_bytes);});
  test("upload size cap",[]{rejects([]{encode(std::string(max_object_bytes+1,'x'));});});
  test("corrupt content rejected",[]{auto p=encode("content");p.bytes.back()^=1;rejects([&]{decode(p.bytes);});});
  test("compressed payload corruption rejected",[]{auto p=encode(std::string(100000,'a'));p.bytes.back()^=127;rejects([&]{decode(p.bytes);});});
  test("truncated header rejected",[]{rejects([]{decode("RTOBJ001");});});
  test("decompression size cap before allocation",[]{auto p=encode("x");p.bytes[9]=127;rejects([&]{decode(p.bytes);});});
  test("unknown codec rejected",[]{auto p=encode("x");p.bytes[8]=7;rejects([&]{decode(p.bytes);});});
  test("trailing payload rejected",[]{auto p=encode("x");p.bytes+='x';rejects([&]{decode(p.bytes);});});
  const auto root=std::filesystem::temp_directory_path()/("runtime-storage-test-"+object_id());
  test("durable object survives store reopen",[&]{const auto id=object_id();{LocalObjectStore s(root);s.put(id,encode("persistent bytes").bytes);}LocalObjectStore again(root);check(decode(again.get(id))=="persistent bytes");});
  test("object overwrite forbidden",[&]{LocalObjectStore s(root);auto id=object_id();s.put(id,encode("first").bytes);rejects([&]{s.put(id,encode("second").bytes);});check(decode(s.get(id))=="first");});
  test("directory traversal forbidden",[&]{LocalObjectStore s(root);rejects([&]{s.get("../../etc/passwd");});});
  test("symlink reads forbidden",[&]{LocalObjectStore s(root);const auto id=object_id();std::filesystem::create_symlink("/etc/passwd",root/id);rejects([&]{s.get(id);});});
  test("private root permissions",[&]{struct stat info{};check(::stat(root.c_str(),&info)==0);check((info.st_mode&0777)==0700);});
  test("checksum deterministic",[]{check(encode("hello").sha256=="2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824");});
  std::filesystem::remove_all(root);std::cout<<passed<<" storage tests passed\n";
}
