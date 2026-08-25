#include "hrd/Imported/ImportedSymbolBuilder.h"
#include "hrd/MetaData/MetaData.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include <cstddef>

void ImportedSymbolBuilder::linkType(TypeMeta &type) {
  auto file = getFile(type.path);
  auto symbol = getTypeSymbol(file, type.name);
  if (type.parent.has_value()) {
    symbol->base = getOrCreateTypeRef(type.parent.value());
  }

  for (auto &f : type.fields) {
    linkField(f);
  }

  for (auto &m : type.methods) {
    linkMethod(m);
  }

  for (auto &v : type.variants) {
    linkVariant(v);
  }
}

void ImportedSymbolBuilder::linkField(FieldMeta &field) {
  auto symbol = getField(field);
  symbol->typeSymbol = getOrCreateTypeRef(field.type);
}

void ImportedSymbolBuilder::linkMethod(MethodMeta &method) {
  auto symbol = getMethod(method);
  symbol->returnType = getOrCreateTypeRef(method.returnType);
  for (size_t i = 0; i < symbol->params.size(); ++i) {
    auto pSymbol = symbol->params[i];
    auto &pMeta = method.params[i];
    pSymbol->typeSymbol = getOrCreateTypeRef(pMeta.type);
  }
}

void ImportedSymbolBuilder::linkVariant(EnumVariantMeta &variant) {
  auto symbol = getVariant(variant);
  if (variant.payload.has_value()) {
    symbol->payloadType = getOrCreateTypeRef(variant.payload.value());
  }
}