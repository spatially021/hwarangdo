#pragma once

#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/RuntimeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

class SymbolRegistry;
class SymbolTable;

class MIRSymbolIndex {
public:
  explicit MIRSymbolIndex(SymbolTable &table);

  void rebuild();

  std::string typeKey(TypeSymbol *symbol);
  std::string methodKey(MethodSymbol *symbol);
  std::string fieldKey(ValueSymbol *symbol);
  std::string variantKey(EnumVariantSymbol *symbol);
  std::string runtimeKey(RuntimeSymbol *symbol);
  std::string genericParamKey(GenericParamSymbol *symbol);

  TypeSymbol *resolveType(std::string_view key);
  MethodSymbol *resolveMethod(std::string_view key) const;
  ValueSymbol *resolveField(std::string_view key) const;
  EnumVariantSymbol *resolveVariant(std::string_view key) const;
  RuntimeSymbol *resolveRuntime(std::string_view key);
  GenericParamSymbol *resolveGenericParam(std::string_view key) const;

private:
  SymbolTable &table;
  SymbolRegistry &registry;

  std::unordered_map<std::string, TypeSymbol *> types;
  std::unordered_map<std::string, MethodSymbol *> methods;
  std::unordered_map<std::string, ValueSymbol *> fields;
  std::unordered_map<std::string, EnumVariantSymbol *> variants;
  std::unordered_map<std::string, GenericParamSymbol *> genericParams;

  std::unordered_map<TypeSymbol *, std::string> typeKeys;
  std::unordered_map<MethodSymbol *, std::string> methodKeys;
  std::unordered_map<ValueSymbol *, std::string> fieldKeys;
  std::unordered_map<EnumVariantSymbol *, std::string> variantKeys;
  std::unordered_map<GenericParamSymbol *, std::string> genericParamKeys;

  std::unordered_set<TypeSymbol *> indexedTypes;
  std::unordered_set<MethodSymbol *> indexedMethods;

  void indexType(TypeSymbol *type);
  void indexMethod(MethodSymbol *method);
  void indexGenericParams(GenericOnwer &owner, std::string_view ownerKey,
                          char ownerKind);

  std::string makeTypeKey(TypeSymbol *symbol);
  std::string makeMethodKey(MethodSymbol *symbol);
  std::string makeFieldKey(TypeSymbol *owner, ValueSymbol *field) const;
  std::string makeVariantKey(EnumType *owner, EnumVariantSymbol *variant) const;

  TypeSymbol *resolveConstructedType(std::string_view key);
};
