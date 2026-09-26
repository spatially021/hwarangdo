#include "hrd/SemanticAnalyzer/SymbolTable/SymbolRegistry.h"
#include "hrd/SemanticAnalyzer/Module.h"
#include "hrd/SemanticAnalyzer/Scope.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/util/Helper.h"
#include <cstddef>
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
    raw->name = origin->name + "<";
    for (size_t i = 0; i < args.size(); ++i) {
      raw->name += args[i]->name;
      if (i < args.size() - 1) {
        raw->name += ",";
      }
    }
    raw->name += ">";
    it = genericInsSMap.emplace(key, raw).first;
    types.push_back(std::move(ins));
    if (!(hasGenericParam(raw))) {
      typeRaw.push_back(raw);
    }
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
    raw->name = base->name + "[" + Helper::apIntToString(size) + "]";
    it = arrayTypeMap.emplace(key, raw).first;
    types.push_back(std::move(type));
    if (!(hasGenericParam(raw))) {
      typeRaw.push_back(raw);
    }
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

bool SymbolRegistry::hasGenericParam(TypeSymbol *symbol) {
  if (isa<GenericParamSymbol>(symbol)) {
    return true;
  }

  if (auto *generic = dyn_cast<GenericSymbol>(symbol)) {
    for (auto *arg : generic->args) {
      if (hasGenericParam(arg)) {
        return true;
      }
    }
  }

  if (auto *array = dyn_cast<ArrayTypeSymbol>(symbol)) {
    return hasGenericParam(array->baseType);
  }

  return false;
}
