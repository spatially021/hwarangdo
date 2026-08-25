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

  auto symbol = make_unique<TypeSymbol>();
  auto sc = make_unique<Scope>();

  symbol->memberScope = sc.get();
  symbol->module = module;
  scope->children.push_back(std::move(sc));

  auto raw = symbol.get();

  map.emplace(type.name, std::move(symbol));

  raw->name = type.name;

  switch (type.kind) {
  case TypeKind::Enum:
    raw->kind = TypeSymbol::TypeKind::ENUM;
    break;
  case TypeKind::Class:
    raw->kind = TypeSymbol::TypeKind::CLASS;
    break;
  case TypeKind::Struct:
    raw->kind = TypeSymbol::TypeKind::STRUCT;
    break;
  }

  for (auto &f : type.fields) {
    raw->fields.push_back(buildField(f, raw));
  }

  for (auto &m : type.methods) {
    buildMethod(m, raw);
  }

  for (auto &v : type.variants) {
    buildVariant(v, raw);
  }
}

ValueSymbol *ImportedSymbolBuilder::buildField(FieldMeta &field,
                                               TypeSymbol *type) {
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
  if (method.name == "init") {
    type->memberScope->initOwn.push_back(std::move(symbol));
    type->memberScope->inits.push_back(raw);
  } else {
    type->memberScope->methodOwn.push_back(std::move(symbol));
    type->memberScope->methodMap[method.name].push_back(raw);
  }
  methodMap[*currnet].emplace(method, raw);
  raw->name = method.name;
  raw->owner = type;
  raw->module = module;
  raw->path = currnet->path;
  unique_ptr<Scope> sc = make_unique<Scope>();
  auto rSc = sc.get();
  rSc->parent = type->memberScope;
  type->memberScope->children.push_back(std::move(sc));
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
                                         TypeSymbol *type) {
  unique_ptr<EnumVariantSymbol> symbol = make_unique<EnumVariantSymbol>();
  auto raw = symbol.get();
  type->variants.push_back(std::move(symbol));
  type->variantMap.emplace(variant.name, raw);
  raw->name = variant.name;
  raw->typeSymbol = type;
  variantMap[*currnet].emplace(variant, raw);
}