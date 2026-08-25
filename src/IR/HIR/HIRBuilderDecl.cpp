#include "hrd/AST/Decl.h"
#include "hrd/IR/HIR/HIRBuilder.h"
#include "hrd/IR/HIR/HIRExpr.h"
#include "hrd/IR/HIR/HIRSymbol.h"
#include <cassert>
#include <memory>
#include <utility>

HIRLocal *HIRBuilder::lowerLocal(VarDecl *decl) {
  unique_ptr<HIRLocal> local = make_unique<HIRLocal>();
  local->symbol = decl->symbol;
  local->name = decl->name;
  local->isInitialized = (decl->init != nullptr);
  local->type = decl->symbol->typeSymbol;
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
  param->type = decl->symbol->typeSymbol;
  if (decl->defaultValue.has_value()) {
    param->defaultValue = lowerExpr(decl->defaultValue.value().get());
  }
  return param;
}