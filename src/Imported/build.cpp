#include "hrd/AST/Expr.h"
#include "hrd/Imported/ImportedSymbolBuilder.h"
#include "hrd/MetaData/MetaData.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/util/Error.h"
#include <memory>
#include <utility>
void ImportedSymbolBuilder::buildType(TypeMeta &type) {
  auto file = getOrCreateFile(type.path);
  auto &map = getTypeMap(file);

  unique_ptr<TypeSymbol> symbol;

  switch (type.kind) {
  case TypeKind::ENUM: {
    symbol = make_unique<EnumType>();
    auto raw = dynamic_cast<EnumType *>(symbol.get());
    for (auto &v : type.variants) {
      buildVariant(v, raw);
    }
    break;
  }
  case TypeKind::CLASS:
  case TypeKind::STRUCT: {
    symbol = make_unique<ObjectType>(type.kind);

    auto sc = make_unique<Scope>();
    auto raw = dynamic_cast<ObjectType *>(symbol.get());
    raw->memberScope = sc.get();
    scope->children.push_back(std::move(sc));
    for (auto &f : type.fields) {
      raw->fields.push_back(buildField(f, raw));
    }
    break;
  }
  default: {
    Error::internal("illegal meta type kind");
  }
  }

  symbol->module = module;
  auto raw = symbol.get();

  map.emplace(type.name, std::move(symbol));

  raw->name = type.name;

  for (auto &m : type.methods) {
    buildMethod(m, raw);
  }
}

ValueSymbol *ImportedSymbolBuilder::buildField(FieldMeta &field,
                                               ObjectType *type) {
  unique_ptr<ValueSymbol> symbol = make_unique<ValueSymbol>();
  auto raw = symbol.get();
  type->memberScope->value.emplace(field.name, std::move(symbol));
  fieldMap[*currnet].emplace(field, raw);
  raw->name = field.name;
  return raw;
}

void ImportedSymbolBuilder::buildMethod(MethodMeta &method, TypeSymbol *type) {
  unique_ptr<MethodSymbol> symbol = make_unique<MethodSymbol>();
  auto raw = symbol.get();
  raw->name = method.name;
  raw->owner = type;
  raw->module = module;
  raw->path = currnet->path;
  raw->isStatic = method.isStatic;
  unique_ptr<Scope> sc = make_unique<Scope>();
  auto rSc = sc.get();
  if (method.name == "init") {
    auto obj = dynamic_cast<ObjectType *>(type);
    if (obj == nullptr) {
      Error::internal("illegal meta type kind");
    }
    obj->addInit(std::move(symbol));
    rSc->parent = obj->memberScope;
    obj->memberScope->children.push_back(std::move(sc));

  } else if (method.name == "onDestroy") {
    if (auto obj = dyn_cast<ObjectType>(type)) {
      obj->onDestroy = std::move(symbol);
    } else {
      Error::internal("illegal onDestroy");
    }
  } else {
    if (isa<ObjectType>(type)) {
      dyn_cast<ObjectType>(type)->addMethod(std::move(symbol), method.isStatic);
    } else {
      type->addMethod(std::move(symbol));
    }
  }
  methodMap[*currnet].emplace(method, raw);

  for (auto &p : method.params) {
    buildParam(p, raw, rSc);
  }
}

void ImportedSymbolBuilder::buildParam(ParamMeta &param, MethodSymbol *method,
                                       Scope *sc) {
  unique_ptr<ParamSymbol> symbol = make_unique<ParamSymbol>();
  auto raw = symbol.get();
  method->params.push_back(raw);
  if (!sc->value.emplace(param.name, std::move(symbol)).second) {
    Error::internal("fail to add param");
  }
  raw->name = param.name;
}

void ImportedSymbolBuilder::buildVariant(EnumVariantMeta &variant,
                                         EnumType *type) {
  unique_ptr<EnumVariantSymbol> symbol = make_unique<EnumVariantSymbol>();
  auto raw = symbol.get();
  type->variants.push_back(std::move(symbol));
  type->variantMap.emplace(variant.name, raw);
  raw->name = variant.name;
  raw->typeSymbol = type;
  variantMap[*currnet].emplace(variant, raw);
}