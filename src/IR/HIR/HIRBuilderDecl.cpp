#include "AST/Decl.h"
#include "IR/HIR/HIRBuilder.h"
#include "IR/HIR/HIRDecl.h"
#include "IR/HIR/HIRExpr.h"
#include "IR/HIR/HIRSymbol.h"
#include "util/Error.h"
#include "util/Guard.h"
#include <cassert>
#include <memory>
#include <utility>
#include <vector>

HIRLocal *HIRBuilder::lowerLocal(VarDecl *decl) {
  unique_ptr<HIRLocal> local = make_unique<HIRLocal>();
  local->symbol = decl->symbol;
  local->name = decl->name;
  local->isInitialized = (decl->init != nullptr);
  local->isMutable = decl->isMutable;
  auto type = lowerType(decl->symbol->typeSymbol);
  if (type == nullptr) {
    Error::internal("type is nullptr");
  }
  local->type = type;
  local->id = allocLocalID();
  auto raw = local.get();
  bindLocal(raw->symbol, std::move(local));
  return raw;
}

unique_ptr<HIRParam> HIRBuilder::lowerParam(Param *decl) {
  unique_ptr<HIRParam> param = make_unique<HIRParam>();
  param->symbol = decl->symbol;
  param->name = decl->symbol->name;
  param->id = allocParamID();
  param->type = lowerType(decl->symbol->typeSymbol);
  if (decl->defaultValue.has_value()) {
    param->defaultValue = lowerExpr(decl->defaultValue.value().get());
  }
  return param;
}

HIRField *HIRBuilder::lowerField(VarDecl *decl) {
  unique_ptr<HIRField> field = make_unique<HIRField>();
  field->symbol = decl->symbol;
  field->name = decl->name;
  field->isInitialized = (decl->init != nullptr);
  field->isMutable = decl->isMutable;
  auto type = lowerType(decl->symbol->typeSymbol);
  if (type == nullptr) {
    Error::internal("type is nullptr");
  }
  field->type = type;
  field->id = allocFieldID();
  auto raw = field.get();
  bindField(decl->symbol, std::move(field));
  return raw;
}

unique_ptr<HIRMethodDecl> HIRBuilder::lowerMethodDecl(FuncDecl *decl) {
  assert(decl);
  assert(decl->methodSymbol);
  auto method = make_unique<HIRMethodDecl>(currentType, allocMethodID(),
                                           decl->name, decl->methodSymbol);

  auto *raw = method.get();

  method->isAsync = false;
  method->isInit = decl->methodSymbol->isInit;
  method->returnType = lowerType(decl->methodSymbol->returnType);
  vector<unique_ptr<HIRParam>> params;
  for (auto &param : decl->params) {
    params.push_back(lowerParam(param.get()));
  }
  method->setParam(std::move(params));
  {
    MethodGuard _(currentMethod, raw);
    method->body = lowerStmtAsBlock(decl->body.get());
  }
  return method;
}

unique_ptr<HIREnumVariant> HIRBuilder::lowerEnumVariant(EnumDecl::Variant *v) {
  unique_ptr<HIREnumVariant> variant = make_unique<HIREnumVariant>();
  variant->id = allocLocalID();
  variant->name = v->name;
  variant->kind =
      (v->payload ? HIREnumVariantKind::Payload : HIREnumVariantKind::Unit);
  variant->symbol = v->symbol;
  variant->owner = currentType;
  if (v->payload) {
    auto it = program->typeDeclMap.find(v->payload->get()->resolved);
    if (it == program->typeDeclMap.end()) {
      Error::internal("fail to find type");
    }
    variant->payloadType = it->second->type;
  }
  return variant;
}