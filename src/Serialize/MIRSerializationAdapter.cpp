#include "hrd/Serialize/MIRSerializationAdapter.h"
#include "hrd/SemanticAnalyzer/SymbolTable/SymbolTable.h"
#include "hrd/Serialize/MIRSymbolIndex.h"

#include <utility>

MIRSerializationAdapterBundle makeMIRSerializationAdapter(SymbolTable &table) {

  MIRSerializationAdapterBundle bundle;
  bundle.index = std::make_shared<MIRSymbolIndex>(table);

  auto index = bundle.index;
  auto &adapter = bundle.adapter;

  adapter.typeKey = [index](TypeSymbol *symbol) {
    return index->typeKey(symbol);
  };
  adapter.resolveType = [index](std::string_view key) {
    return index->resolveType(key);
  };

  adapter.methodKey = [index](MethodSymbol *symbol) {
    return index->methodKey(symbol);
  };
  adapter.resolveMethod = [index](std::string_view key) {
    return index->resolveMethod(key);
  };

  adapter.fieldKey = [index](ValueSymbol *symbol) {
    return index->fieldKey(symbol);
  };
  adapter.resolveField = [index](std::string_view key) {
    return index->resolveField(key);
  };

  adapter.variantKey = [index](EnumVariantSymbol *symbol) {
    return index->variantKey(symbol);
  };
  adapter.resolveVariant = [index](std::string_view key) {
    return index->resolveVariant(key);
  };

  adapter.runtimeKey = [index](RuntimeSymbol *symbol) {
    return index->runtimeKey(symbol);
  };
  adapter.resolveRuntime = [index](std::string_view key) {
    return index->resolveRuntime(key);
  };

  adapter.genericParamKey = [index](GenericParamSymbol *symbol) {
    return index->genericParamKey(symbol);
  };
  adapter.resolveGenericParam = [index](std::string_view key) {
    return index->resolveGenericParam(key);
  };

  adapter.ownLocal = [&table](std::unique_ptr<ValueSymbol> symbol) {
    auto *raw = symbol.get();
    table.registry.addTemp(std::move(symbol));
    return raw;
  };

  return bundle;
}
