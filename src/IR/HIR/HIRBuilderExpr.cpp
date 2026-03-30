#include "IR/HIR/HIRBuilder.h"
#include "IR/HIR/HIRExpr.h"
#include "IR/HIR/HIRSymbol.h"
#include "IR/HIR/HIRType.h"
#include "util/Error.h"
#include <memory>

unique_ptr<HIRExpr> HIRBuilder::lowerExpr(Expr *expr) {
  if (expr == nullptr) {
    Error::internal("lowering expr is nullptr");
  }
  exprResult.reset();
  expr->accept(this);
  if (exprResult == nullptr) {
    Error::internal("expr result nullptr");
  }
  return std::move(exprResult);
}

unique_ptr<HIRPlaceExpr> HIRBuilder::lowerPlace(NameExpr *expr) {
  if (expr == nullptr) {
    Error::internal("nameExpr is nullptr");
  }
  if (expr->valueSymbol == nullptr) {
    Error::internal(expr->token, "valueSymbol is nullptr");
  }

  auto value = expr->valueSymbol;

  if (auto [cond, result] = lookupLocal(value); cond) {
    return make_unique<HIRLocalPlaceExpr>(result);
  }
  if (auto [cond, result] = lookupParam(value); cond) {
    return make_unique<HIRParamPlaceExpr>(result);
  }
  if (auto [cond, result] = lookupField(value); cond) {

    auto type = lowerType(expr->valueSymbol->typeSymbol);
    if (type == nullptr) {
      Error::internal(expr->token, "hirType is nullptr");
    }

    HIRFieldAccessMode mode = HIRFieldAccessMode::ValueObject;
    if (currentType->type->kind == HIRTypeKind::Entity) {
      mode = HIRFieldAccessMode::ObserverObject;
    }
    return make_unique<HIRFieldPlaceExpr>(currentType, result, mode);
  }
  Error::internal(expr->token, "unregisitered value");
}
