#include "hrd/Imported/ImportedSymbolBuilder.h"
#include "hrd/MetaData/MetaData.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/util/Error.h"
#include <cstddef>

void ImportedSymbolBuilder::linkType(TypeMeta &type) {
  auto file = getFile(type.path);
  auto symbol = getTypeSymbol(file, type.name);
  if (type.parent.has_value()) {

    auto obj = dynamic_cast<ObjectType *>(symbol);
    if (obj == nullptr) {
      Error::internal("illegal type kind");
    }
    auto r = getOrCreateTypeRef(type.parent.value());
    if (!isa<ObjectType>(r)) {
      Error::internal("illegal type kind");
    }
    obj->base = dyn_cast<ObjectType>(r);
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

  if (method.name != "init") {
    return;
  }

  if (symbol->owner == nullptr) {
    Error::internal("imported init has invalid owner");
  }

  auto &fields = summary.initFields[symbol];
  auto obj = dynamic_cast<ObjectType *>(symbol->owner);
  if (!method.initializedFields.empty() && obj == nullptr) {
    Error::internal("illegal type kind");
  }

  for (const auto &fieldName : method.initializedFields) {
    auto it = obj->memberScope->value.find(fieldName);

    if (it == obj->memberScope->value.end()) {
      Error::internal("failed to resolve initialized field '" + fieldName +
                      "' in type '" + symbol->owner->name + "'");
    }

    fields.insert(it->second.get());
  }
}

void ImportedSymbolBuilder::linkVariant(EnumVariantMeta &variant) {
  auto symbol = getVariant(variant);
  if (variant.payload.has_value()) {
    symbol->payloadType = getOrCreateTypeRef(variant.payload.value());
  }
}