
#include "IR/HIR/HIRBuilder.h"
#include "IR/HIR/HIRExpr.h"
#include "util/Error.h"
#include <cassert>
#include <memory>
using std::unique_ptr;

unique_ptr<HIRSelfExpr> HIRBuilder::lowerImplictSelf() {
  assert(currentType);

  auto type = currentType->type;

  return make_unique<HIRSelfExpr>(
      type->kind == HIRTypeKind::Struct ? HIRSelfKind::Self : HIRSelfKind::This,
      type, type, type);
}

HIRLocal *HIRBuilder::makeTemp(HIRType *type) {
  auto local = make_unique<HIRLocal>();
  local->id = allocLocalID();
  local->type = type;
  local->symbol = nullptr;
  local->kind = HIRLocalKind::Temp;
  local->isMutable = false;
  local->isInitialized = false;
  local->name = "";
  auto raw = local.get();

  currentMethod->locals.push_back(std::move(local));

  return raw;
}

HIRLocal *HIRBuilder::lookUpLocal(ValueSymbol *symbol) {
  assert(currentBlock);
  auto it = currentBlock->localMap.find(symbol);
  if (it == currentBlock->localMap.end()) {
    return nullptr;
  }
  return it->second;
}

unique_ptr<HIRValueExpr> HIRBuilder::lowerValue(Expr *expr) {
  exprResult = lowerExpr(expr);
  unique_ptr<HIRValueExpr> rt = nullptr;
  if (dynamic_cast<HIRValueExpr *>(exprResult.get())) {
    rt = unique_ptr<HIRValueExpr>(
        static_cast<HIRValueExpr *>(exprResult.release()));
  } else if (dynamic_cast<HIRPlaceExpr *>(exprResult.get())) {
    rt = load(unique_ptr<HIRPlaceExpr>(
        static_cast<HIRPlaceExpr *>(exprResult.release())));
  } else {
    Error::internal("expect value or place type");
  }
  if (rt == nullptr) {
    Error::internal(expr->token, "fail to get value");
  }
  return rt;
}