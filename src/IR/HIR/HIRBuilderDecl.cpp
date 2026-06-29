#include "hrd/AST/Decl.h"
#include "hrd/IR/HIR/HIRBuilder.h"
#include "hrd/IR/HIR/HIRExpr.h"
#include "hrd/IR/HIR/HIRHelper.h"
#include "hrd/IR/HIR/HIRSymbol.h"
#include "hrd/util/Error.h"
#include <cassert>
#include <memory>
#include <utility>

HIRLocal *HIRBuilder::lowerLocal(VarDecl *decl) {
  unique_ptr<HIRLocal> local = make_unique<HIRLocal>();
  local->symbol = decl->symbol;
  local->name = decl->name;
  local->isInitialized = (decl->init != nullptr);
  local->isMutable = decl->isMutable;
  auto type = HIRHelper::lowerType(program, source, decl->symbol->typeSymbol);
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
  param->type = HIRHelper::lowerType(program, source, decl->symbol->typeSymbol);
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
  auto type = HIRHelper::lowerType(program, source, decl->symbol->typeSymbol);
  if (type == nullptr) {
    Error::internal("type is nullptr");
  }
  field->type = type;
  field->id = allocFieldID();
  auto raw = field.get();
  bindField(decl->symbol, std::move(field));
  return raw;
}
