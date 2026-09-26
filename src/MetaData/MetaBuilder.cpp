#include "hrd/MetaData/MetaBuilder.h"

#include "hrd/AST/Decl.h"
#include "hrd/AST/Expr.h"
#include "hrd/AST/Stmt.h"
#include "hrd/BuiltInType.h"
#include "hrd/InitChecker/InitSummary.h"
#include "hrd/MetaData/MetaData.h"
#include "hrd/MetaData/TypeRef.h"
#include "hrd/SemanticAnalyzer/SymbolTable/SymbolTable.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/compiler/CompilerContexts.h"
#include "hrd/util/Error.h"

#include <algorithm>
#include <variant>

MetaBuilder::MetaBuilder(MetaBuilderContext &ctx)
    : table(ctx.table), summary(ctx.summary) {}

ModuleMeta MetaBuilder::build() {
  ModuleMeta module;

  for (auto type : table.registry.getDecledTypes()) {
    if (type->kind == TypeKind::TRAIT) {
      module.traits.push_back(buildTrait(type));
    }

    if (type->kind == TypeKind::CLASS || type->kind == TypeKind::STRUCT ||
        type->kind == TypeKind::ENUM) {
      module.types.push_back(buildType(type));
    }
  }

  return module;
}

TypeMeta MetaBuilder::buildType(TypeSymbol *type) {
  TypeMeta meta;

  meta.name = type->name;
  meta.path = type->path;
  for (auto *param : type->getGenericParams()) {
    if (param == nullptr) {
      Error::internal("type has null generic parameter: " + type->name);
    }
    meta.genericParams.push_back(param->name);
  }

  if (type->isGenericDecl != !meta.genericParams.empty()) {
    Error::internal("type generic declaration and parameters disagree: " +
                    type->name);
  }

  if (type->kind == TypeKind::CLASS || type->kind == TypeKind::STRUCT ||
      type->kind == TypeKind::ENUM) {
    meta.kind = type->kind;
  } else {
    Error::internal("illegal type kind");
  }

  for (auto m : type->getMethods()) {
    meta.methods.push_back(buildMethod(m));
  }

  if (auto obj = dynamic_cast<ObjectType *>(type)) {
    for (auto f : obj->fields) {
      meta.fields.push_back(buildField(f));
    }

    for (auto i : obj->getInits()) {
      meta.methods.push_back(buildMethod(i));
    }

    if (obj->base != nullptr) {
      meta.parent = buildTypeRef(obj->base);
    }

    for (auto t : obj->traits) {
      meta.traits.push_back(buildTypeRef(t));
    }
  }

  if (auto en = dynamic_cast<EnumType *>(type)) {
    for (auto &v : en->variants) {
      meta.variants.push_back(buildVariant(v.get()));
    }
  }

  return meta;
}

TraitMeta MetaBuilder::buildTrait(TypeSymbol *type) {
  auto tra = dynamic_cast<TraitType *>(type);
  if (tra == nullptr) {
    Error::internal("illegal type kind");
  }

  TraitMeta meta;

  meta.name = type->name;
  meta.path = type->path;

  for (auto &sig : tra->traitSigs) {
    for (auto m : sig.second) {
      meta.methods.push_back(buildMethod(m));
    }
  }

  return meta;
}

FieldMeta MetaBuilder::buildField(ValueSymbol *symbol) {
  FieldMeta meta;

  meta.name = symbol->name;
  meta.modifier = symbol->modifier;
  meta.type = buildTypeRef(symbol->typeSymbol);

  return meta;
}

MethodMeta MetaBuilder::buildMethod(MethodSymbol *symbol) {
  MethodMeta meta;

  meta.name = symbol->name;
  meta.modifier = symbol->modifier;
  meta.isStatic = symbol->isStatic;
  meta.isGenericDecl = symbol->isGenericDecl;
  for (auto *param : symbol->getGenericParams()) {
    if (param == nullptr) {
      Error::internal("method has null generic parameter: " + symbol->name);
    }
    meta.genericParams.push_back(param->name);
  }

  if (meta.isGenericDecl != !meta.genericParams.empty()) {
    Error::internal("method generic declaration and parameters disagree: " +
                    symbol->name);
  }

  if (auto func = dynamic_cast<FuncDecl *>(symbol->decl)) {
    for (auto &p : func->params) {
      meta.params.push_back(buildParam(p.get()));
    }
  } else if (auto sig = dynamic_cast<TraitSig *>(symbol->decl)) {
    for (auto &p : sig->params) {
      meta.params.push_back(buildParam(p.get()));
    }
  } else {
    Error::internal("illegal ast kind");
  }

  meta.returnType = buildTypeRef(symbol->returnType);

  auto init = summary.initFields.find(symbol);

  if (init != summary.initFields.end()) {
    for (auto *field : init->second) {
      if (field == nullptr) {
        Error::internal("init summary contains nullptr field");
      }

      meta.initializedFields.push_back(field->name);
    }
  }

  std::sort(meta.initializedFields.begin(), meta.initializedFields.end());

  return meta;
}

ParamMeta MetaBuilder::buildParam(Param *param) {
  ParamMeta meta;

  meta.name = param->name;
  meta.type = buildTypeRef(param->type->resolved);

  if (param->defaultValue.has_value()) {
    meta.defaultValue = buildDefaultValue(param->defaultValue.value().get());
  }

  return meta;
}

DefaultValueMeta MetaBuilder::buildDefaultValue(Expr *expr) {
  DefaultValueMeta meta;

  if (auto lit = dynamic_cast<LiteralExpr *>(expr)) {
    meta.kind = DefaultValueKind::Literal;
    meta.literal = lit->resolvedLit;
    meta.resolvedType = buildTypeRef(expr->resolvedType);

  } else if (auto init = dynamic_cast<CallExpr *>(expr)) {
    if (init->callType != CallExpr::CallType::INIT_CALL) {
      Error::internal("illegal call kind");
    }

    meta.kind = DefaultValueKind::StructInit;
    meta.type = buildTypeRef(init->resolvedType);
    meta.resolvedType = buildTypeRef(expr->resolvedType);

    for (auto a : init->arguments) {
      meta.args.push_back(buildDefaultValue(a.get()));
    }

  } else if (auto value = dynamic_cast<DefaultValueExpr *>(expr)) {
    if (auto l = get_if<LiteralExpr *>(&value->resolved)) {
      meta.kind = DefaultValueKind::Literal;
      meta.literal = (*l)->resolvedLit;
      meta.resolvedType = buildTypeRef((*l)->resolvedType);

    } else if (auto i = get_if<CallExpr *>(&value->resolved)) {
      if ((*i)->callType != CallExpr::CallType::INIT_CALL) {
        Error::internal("illegal call kind");
      }

      meta.kind = DefaultValueKind::StructInit;
      meta.type = buildTypeRef((*i)->resolvedType);
      meta.resolvedType = buildTypeRef((*i)->resolvedType);

      for (auto a : (*i)->arguments) {
        meta.args.push_back(buildDefaultValue(a.get()));
      }

    } else {
      Error::internal("unknown default value kind");
    }

  } else {
    Error::internal("fail to get defaultValue");
  }

  return meta;
}

EnumVariantMeta MetaBuilder::buildVariant(EnumVariantSymbol *symbol) {
  EnumVariantMeta meta;

  meta.name = symbol->name;

  if (symbol->payloadType) {
    meta.payload = buildTypeRef(symbol->payloadType);
  }

  return meta;
}

TypeRef MetaBuilder::buildTypeRef(TypeSymbol *symbol) {
  TypeRef ref;

  if (symbol == nullptr) {
    Error::internal("typeSymbol is nullptr");
  }

  switch (symbol->kind) {
  case TypeKind::CLASS:
  case TypeKind::ENUM:
  case TypeKind::STRUCT:
  case TypeKind::TRAIT: {
    ref.name = symbol->name;
    ref.kind = TypeRefKind::Declared;
    ref.path = symbol->path;
    break;
  }

  case TypeKind::VOID: {
    ref.kind = TypeRefKind::BuiltIn;
    ref.builtIn = BuiltInType::VOID;
    break;
  }

  case TypeKind::PRIMITIVE:
  case TypeKind::BUILTIN: {
    ref.kind = TypeRefKind::BuiltIn;

    auto built = dynamic_cast<PrimtiveType *>(symbol);

    if (built == nullptr) {
      Error::internal("illegal type : " + symbol->name);
    }

    ref.builtIn = built->builtinType;
    break;
  }

  case TypeKind::ARRAY: {
    ref.kind = TypeRefKind::Array;

    auto arr = dynamic_cast<ArrayTypeSymbol *>(symbol);

    if (arr == nullptr) {
      Error::internal("illegal array type : " + symbol->name);
    }

    ref.arraySize = arr->sizeValue;
    ref.args.push_back(buildTypeRef(arr->baseType));
    break;
  }

  case TypeKind::GENERIC_PARAM: {
    ref.kind = TypeRefKind::GenericParam;
    ref.name = symbol->name;
    break;
  }

  case TypeKind::GENERIC: {
    ref.kind = TypeRefKind::Generic;

    auto gen = dynamic_cast<GenericSymbol *>(symbol);

    if (gen == nullptr || gen->origin == nullptr) {
      Error::internal("illegal generic type : " + symbol->name);
    }

    ref.name = gen->origin->name;
    ref.path = gen->origin->path;

    for (auto a : gen->args) {
      ref.args.push_back(buildTypeRef(a));
    }

    break;
  }

  default:
    Error::internal("unknown type kind : " + symbol->name);
  }

  return ref;
}
