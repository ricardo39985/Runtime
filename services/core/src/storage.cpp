#include "runtime/storage.hpp"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <zstd.h>
namespace runtime::apps {
std::string hex(std::string_view value) {
  static constexpr char digits[]="0123456789abcdef"; std::string result; result.reserve(value.size()*2);
  for(unsigned char c:value) {result+=digits[c>>4]; result+=digits[c&15];} return result;
}
std::string unhex(std::string_view value) {
  need(value.size()%2==0 && value.size()<=max_asset_bytes*2,"corrupt_object","Invalid stored object encoding.");
  auto nibble=[](char c){if(c>='0'&&c<='9')return c-'0'; if(c>='a'&&c<='f')return c-'a'+10; throw Fault("corrupt_object","Invalid hex digit.");};
  std::string result; result.reserve(value.size()/2); for(std::size_t i=0;i<value.size();i+=2) result+=static_cast<char>(nibble(value[i])*16+nibble(value[i+1])); return result;
}
std::string hash(std::string_view value) {
  unsigned char digest[EVP_MAX_MD_SIZE]; unsigned int length=0;
  need(EVP_Digest(value.data(),value.size(),digest,&length,EVP_sha256(),nullptr)==1,"storage_error","Digest failed.");
  return hex(std::string_view(reinterpret_cast<const char*>(digest),length));
}
std::string new_id() {
  char bytes[16]; need(RAND_bytes(reinterpret_cast<unsigned char*>(bytes),sizeof bytes)==1,"storage_error","Secure randomness unavailable."); return hex(std::string_view(bytes,sizeof bytes));
}
StoredObject encode(std::string_view original) {
  need(original.size()<=max_asset_bytes,"input_limit","File exceeds 8 MiB.");
  StoredObject object{hash(original),"identity",std::string(original),original.size()};
  if(original.size()>=256) {
    std::string compressed(ZSTD_compressBound(original.size()),'\0');
    auto size=ZSTD_compress(compressed.data(),compressed.size(),original.data(),original.size(),3);
    need(!ZSTD_isError(size),"storage_error","Compression failed.");
    // Store compressed bytes only when they actually save space. Originals are losslessly recoverable.
    if(size+64 < original.size() && size*100 <= original.size()*95) {compressed.resize(size); object.codec="zstd"; object.bytes=std::move(compressed);}
  }
  return object;
}
std::string decode(const StoredObject& object) {
  need(object.original_size<=max_asset_bytes && object.bytes.size()<=max_asset_bytes,"corrupt_object","Stored object exceeds decoded-size limits.");
  std::string original;
  if(object.codec=="identity") {need(object.bytes.size()==object.original_size,"corrupt_object","Stored size mismatch."); original=object.bytes;}
  else if(object.codec=="zstd") {
    need(ZSTD_getFrameContentSize(object.bytes.data(),object.bytes.size())==object.original_size,"corrupt_object","Invalid decoded size.");
    const auto frame=ZSTD_findFrameCompressedSize(object.bytes.data(),object.bytes.size());
    need(!ZSTD_isError(frame) && frame==object.bytes.size(),"corrupt_object","Invalid or concatenated compression frames.");
    original.resize(object.original_size);
    auto size=ZSTD_decompress(original.data(),original.size(),object.bytes.data(),object.bytes.size());
    need(!ZSTD_isError(size) && size==object.original_size,"corrupt_object","Decompression failed.");
  } else throw Fault("corrupt_object","Unknown storage codec.");
  need(hash(original)==object.sha256,"corrupt_object","File integrity check failed."); return original;
}
std::string safe_filename(std::string_view name) {
  need(!name.empty() && name.size()<=180,"invalid_file","Filename must contain 1–180 bytes.");
  for(unsigned char c:name) need(c>=32 && c!=127 && c!='/' && c!='\\' && c!='"',"invalid_file","Invalid filename.");
  need(name!="." && name!="..","invalid_file","Invalid filename."); return std::string(name);
}
} // namespace runtime::apps
