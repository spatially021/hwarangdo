#include "hrd/SemanticAnalyzer/SymbolTable/SymbolRegistry.h"
#include "hrd/SemanticAnalyzer/Module.h"
#include "hrd/SemanticAnalyzer/Scope.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include <memory>
#include <utility>

SymbolRegistry::SymbolRegistry() {}

SymbolRegistry::~SymbolRegistry() = default;

GenericSymbol *
SymbolRegistry::getOrCreateGeneric(TypeSymbol *origin,
                                   std::vector<TypeSymbol *> args) {
  auto key = GenericInsKey({origin, args});
  auto it = genericInsSMap.find(key);
  if (it == genericInsSMap.end()) {
    auto ins = make_unique<GenericSymbol>(origin, args);
    auto raw = ins.get();
    it = genericInsSMap.emplace(key, raw).first;
    types.push_back(std::move(ins));
  }
  return it->second;
}

ArrayTypeSymbol *SymbolRegistry::getOrCreateArray(TypeSymbol *base,
                                                  llvm::APInt size) {
  auto key = ArrayTypeKey({base, size});
  auto it = arrayTypeMap.find(key);
  if (it == arrayTypeMap.end()) {
    auto type = make_unique<ArrayTypeSymbol>(base, size);
    auto raw = type.get();
    it = arrayTypeMap.emplace(key, raw).first;
    types.push_back(std::move(type));
  }
  return it->second;
}

ValueSymbol *SymbolRegistry::createPayload(TypeSymbol *type) {
  unique_ptr<ValueSymbol> symbol = make_unique<ValueSymbol>();
  symbol->typeSymbol = type;
  symbol->isPayload = true;
  auto raw = symbol.get();
  payloadSymbols.push_back(std::move(symbol));
  return raw;
}

void SymbolRegistry::setModule(Module *m) { currentModule = m; }