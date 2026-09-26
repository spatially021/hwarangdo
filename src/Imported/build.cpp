#include "hrd/AST/Expr.h"
#include "hrd/Imported/ImportedSymbolBuilder.h"
#include "hrd/MetaData/MetaData.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/util/Error.h"
#include <cstddef>
#include <cstdint>
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
    auto sc = make_unique<Scope>();
    raw->scope = sc.get();
    scope->children.push_back(std::move(sc));
    for (uint32_t i = 0; i < type.variants.size(); ++i) {
      buildVariant(type.variants[i], raw, i);
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
  raw->path = type.path;
  raw->isGenericDecl = !type.genericParams.empty();

  for (size_t i = 0; i < type.genericParams.size(); ++i) {
    auto param = make_unique<GenericParamSymbol>(type.genericParams[i], raw, i);

    if (!raw->addGenericParam(std::move(param))) {
      Error::internal("duplicate imported generic parameter: " +
                      type.genericParams[i]);
    }
  }

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
  raw->modifier = field.modifier;
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
  raw->modifier = method.modifier;
  raw->isGenericDecl = method.isGenericDecl;

  if (method.isGenericDecl != !method.genericParams.empty()) {
    Error::internal("invalid imported generic method: " + method.name);
  }

  for (size_t i = 0; i < method.genericParams.size(); ++i) {
    auto param =
        make_unique<GenericParamSymbol>(method.genericParams[i], raw, i);

    if (!raw->addGenericParam(std::move(param))) {
      Error::internal("duplicate imported generic parameter: " +
                      method.genericParams[i]);
    }
  }
  unique_ptr<Scope> sc = make_unique<Scope>();
  auto rSc = sc.get();
  rSc->parent = scope;
  scope->children.push_back(std::move(sc));
  if (method.name == "init") {
    auto obj = dynamic_cast<ObjectType *>(type);
    if (obj == nullptr) {
      Error::internal("illegal meta type kind");
    }
    obj->addInit(std::move(symbol));

  } else if (method.name == "onDestroy") {
    if (auto obj = dyn_cast<ObjectType>(type)) {
      obj->onDestroy = std::move(symbol);

    } else {
      Error::internal("illegal onDestroy");
    }

  } else {
    if (isa<ObjectType>(type)) {
      auto obj = dyn_cast<ObjectType>(type);
      auto [result, _] = obj->addMethod(std::move(symbol), method.isStatic);
      if (!result) {
        Error::internal("fail to add import type's method");
      }
    } else {
      auto [result, _] = type->addMethod(std::move(symbol));
      if (!result) {
        Error::internal("fail to add import type's method");
      }
    }
  }
  methodMap[type].emplace(method, raw);

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
                                         EnumType *type, uint32_t ordinal) {
  unique_ptr<EnumVariantSymbol> symbol = make_unique<EnumVariantSymbol>();
  auto raw = symbol.get();
  type->variants.push_back(std::move(symbol));
  type->variantMap.emplace(variant.name, raw);
  raw->name = variant.name;
  raw->typeSymbol = type;
  raw->ordinal = ordinal;
  variantMap[*currnet].emplace(variant, raw);
}

void ImportedSymbolBuilder::buildTrait(TraitMeta &ref) {
  auto *file = getOrCreateFile(ref.path);
  auto &map = getTypeMap(file);

  auto symbol = make_unique<TraitType>();
  auto *raw = symbol.get();

  raw->name = ref.name;
  raw->path = ref.path;
  raw->module = module;

  if (!map.emplace(ref.name, std::move(symbol)).second) {
    Error::internal("duplicate imported trait: " + ref.name);
  }

  for (auto &method : ref.methods) {
    buildTraitMethod(method, raw);
  }
}

void ImportedSymbolBuilder::buildTraitMethod(MethodMeta &method,
                                             TraitType *trait) {
  auto symbol = make_unique<MethodSymbol>();
  auto *raw = symbol.get();

  raw->name = method.name;
  raw->owner = trait;
  raw->module = module;
  raw->path = trait->path;
  raw->modifier = method.modifier;
  raw->isStatic = false;
  raw->isGenericDecl = method.isGenericDecl;

  auto sc = make_unique<Scope>();
  auto *rSc = sc.get();

  rSc->parent = scope;
  scope->children.push_back(std::move(sc));

  raw->scope = rSc;

  for (size_t i = 0; i < method.genericParams.size(); ++i) {
    if (!raw->addGenericParam(
            make_unique<GenericParamSymbol>(method.genericParams[i], raw, i))) {
      Error::internal("duplicate imported trait generic parameter: " +
                      method.genericParams[i]);
    }
  }

  for (auto &param : method.params) {
    buildParam(param, raw, rSc);
  }

  // MethodSymbol ownership
  trait->addMethod(std::move(symbol));

  // trait signature lookup
  trait->traitSigs[method.name].push_back(raw);

  methodMap[trait].emplace(method, raw);
}