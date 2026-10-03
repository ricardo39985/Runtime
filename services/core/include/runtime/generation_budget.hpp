#pragma once
#include "runtime/application.hpp"
namespace runtime::generation {
// Integer microdollars. Rates are configured account prices per million tokens,
// not hardcoded vendor prices. Conservative input reservation includes byte count
// and an explicit protocol overhead allowance; provider invoice reconciliation is
// still required when metadata is missing or rates/aliases differ.
inline std::uint64_t cost(std::uint64_t tokens,std::uint64_t rate) {
  application::ensure(tokens<=1000000 && rate>0 && rate<=10000000000ULL,"invalid_price","Invalid bounded price or token count.");
  return (tokens*rate+999999)/1000000;
}
inline std::uint64_t reserve(std::size_t request_bytes,std::uint64_t input_rate,std::uint64_t output_rate,std::uint64_t ceiling) {
  application::ensure(request_bytes<=256*1024,"document_limit","Composition input exceeds its limit.");
  const auto amount=cost(request_bytes+4096,input_rate)+cost(8192,output_rate);
  application::ensure(ceiling>0 && ceiling<=1000000 && amount<=ceiling,"budget_limit","This full application request exceeds the configured generation allowance; increase the approved allowance rather than silently reducing the product.");
  return amount;
}
}
