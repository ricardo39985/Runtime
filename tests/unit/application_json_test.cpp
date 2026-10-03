#include "runtime/application_json.hpp"
#include <fstream>
#include <iostream>
int main(int argc,char** argv) {
  using namespace runtime::application;
  try {
    ensure(argc==3,"test_config","Pass the two example specification paths.");
    for(int i=1;i<argc;++i){std::ifstream file(argv[i]);std::string text((std::istreambuf_iterator<char>(file)),{});const auto spec=spec_from_json(read_document(text));ensure(!spec.entities.empty(),"test_failed","No entities.");}
    bool rejected=false;try{read_document("{\"a\":1,\"a\":2}");}catch(const Invalid&){rejected=true;}ensure(rejected,"test_failed","Duplicate JSON keys were accepted.");
    auto integers=record_from_json(read_document("{\"amount\":\"9007199254740993\",\"enabled\":false}"));ensure(std::get<std::string>(integers.at("amount"))=="9007199254740993","test_failed","Integer precision lost.");
    std::cout<<"Application JSON contract examples and negative checks passed\n";
  }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
