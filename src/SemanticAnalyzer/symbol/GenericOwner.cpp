
#include "hrd/SemanticAnalyzer/symbol/GenericOwner.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"

bool GenericOnwer::addGenericParam(unique_ptr<GenericParamSymbol> symbol) {
  auto raw = symbol.get();
  auto [_, insert] = genericParamMap.emplace(raw->name, raw);
  genericParamOwn.push_back(std::move(symbol));
  genericParams.push_back(raw);
  return insert;
}