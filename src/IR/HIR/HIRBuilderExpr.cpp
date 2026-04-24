<<<<<<< HEAD
#include "AST/Decl.h"
#include "AST/Expr.h"
#include "IR/HIR/HIRBuilder.h"
#include "IR/HIR/HIRExpr.h"
#include "IR/HIR/HIRSymbol.h"
#include "SemanticAnalyzer/symbol/MethodSymbol.h"
#include "SemanticAnalyzer/symbol/Symbol.h"
#include "util/Error.h"
#include <cassert>
#include <cstddef>
#include <memory>
#include <utility>
=======
#include "IR/HIR/HIRBuilder.h"
#include "IR/HIR/HIRExpr.h"
#include "IR/HIR/HIRSymbol.h"
#include "IR/HIR/HIRType.h"
#include "util/Error.h"
#include <memory>
>>>>>>> dd7486765f77f2c69a9a91ef8a8c9197d27e5710

unique_ptr<HIRExpr> HIRBuilder::lowerExpr(Expr *expr) {
  if (expr == nullptr) {
    Error::internal("lowering expr is nullptr");
  }
  exprResult.reset();
  expr->accept(this);
  if (exprResult == nullptr) {
<<<<<<< HEAD
    Error::internal(expr->token, "expr result nullptr");
=======
    Error::internal("expr result nullptr");
>>>>>>> dd7486765f77f2c69a9a91ef8a8c9197d27e5710
  }
  return std::move(exprResult);
}

<<<<<<< HEAD
unique_ptr<HIRExpr> HIRBuilder::lowerImplictCall(CallExpr *expr) {
  auto receiver = lowerImplictSelf();
  if (receiver == nullptr) {
    Error::internal(expr->token, "fail to lower implict self");
  }
  HIRMethodDecl *methodDecl = nullptr;
  if (auto method = dynamic_cast<MethodSymbol *>(expr->resolved)) {
    auto it = currentType->methodMap.find(method);
    if (it == currentType->methodMap.end()) {
      Error::internal("fail to find methodDecl");
    }
    methodDecl = it->second;
  } else {
    Error::internal(expr->token, "method call but not methodSymbol");
  }

  vector<unique_ptr<HIRExpr>> args;

  for (auto &a : expr->arguments) {
    args.push_back(lowerExpr(a.get()));
  }

  auto it = program->typeCache.find(expr->resolvedType);

  if (it == program->typeCache.end()) {
    Error::internal("fail to find return type");
  }

  return make_unique<HIRMethodCallExpr>(std::move(receiver), methodDecl,
                                        std::move(args), it->second);
}

std::unique_ptr<HIRExpr> HIRBuilder::lowerCall(CallExpr *expr) {
  assert(expr);
  std::unique_ptr<HIRValueExpr> receiver = lowerReceiver(expr->receiver.get());
  if (receiver == nullptr) {
    Error::internal(expr->token, "fail to lower receiver");
  }
  HIRMethodDecl *methodDecl = nullptr;
  if (auto method = dynamic_cast<MethodSymbol *>(expr->resolved)) {
    auto typeIt = program->typeDeclMap.find(expr->receiver->resolvedType);
    if (typeIt == program->typeDeclMap.end()) {
      Error::internal("fail to get receiver's typeDecl");
    }
    auto it = typeIt->second->methodMap.find(method);
    if (it == currentType->methodMap.end()) {
      Error::internal(expr->token, "fail to find methodDecl");
    }
    methodDecl = it->second;
  } else {
    Error::internal(expr->token, "method call but not methodSymbol");
  }

  vector<unique_ptr<HIRExpr>> args;
  for (size_t i = 0; i < expr->arguments.size(); ++i) {
    auto decl = dynamic_cast<FuncDecl *>(
        dynamic_cast<MethodSymbol *>(expr->resolved)->decl);
    args.push_back(
        lowerCallArg(expr->arguments[i].get(), decl->params[i].get()));
  }

  auto it = program->typeCache.find(expr->resolvedType);

  if (it == program->typeCache.end()) {
    Error::internal("fail to find return type");
  }
  return make_unique<HIRMethodCallExpr>(std::move(receiver), methodDecl,
                                        std::move(args), it->second);
}

unique_ptr<HIRValueExpr> HIRBuilder::lowerCallArg(Expr *expr, Param *param) {
  if (dynamic_cast<DefaultValueExpr *>(expr)) {
    if (!param->defaultValue.has_value()) {
      Error::internal(expr->token, "parameter has no default value");
    }
    return lowerValue(param->defaultValue.value().get());
  }
  return lowerValue(expr);
}

unique_ptr<HIRExpr> HIRBuilder::lowerAssign(AssignExpr *expr) {
  unique_ptr<HIRPlaceExpr> lhs = nullptr;
  if (auto name = dynamic_cast<NameExpr *>(expr->target.get())) {
    // nameExpr -> place
    lhs = lowerPlace(name);
  } else if (auto member = dynamic_cast<MemberExpr *>(expr->target.get())) {
    // memberExpr -> fieldplace
    lhs = lowerMember(member);
  } else if (auto array = dynamic_cast<ArrayAccessExpr *>(expr->target.get())) {
    lhs = lowerArrayAccess(array);
  } else {
    Error::internal(expr->token, "lhs is not nameExpr");
  }

  // expr->hirvalueExpr
  // not allowed nullptr
  unique_ptr<HIRValueExpr> rhs = lowerValue(expr->value.get());

  return make_unique<HIRAssignExpr>(std::move(lhs), std::move(rhs));
}

unique_ptr<HIRExpr> HIRBuilder::lowerTernary(TernaryExpr *expr) {
  // expr->hirValue
  // nullptr not allowed
  auto cond = lowerValue(expr->conditon.get());
  auto then = lowerValue(expr->then.get());
  auto else_ = lowerValue(expr->else_.get());

  // typeSymbol->hirType
  // nullptr not allowed
  auto type = lowerType(expr->resolvedType);

  return make_unique<HIRTernaryExpr>(std::move(cond), std::move(then),
                                     std::move(else_), type);
}

unique_ptr<HIRExpr> HIRBuilder::lowerCast(CastExpr *expr) {
  auto operand = lowerValue(expr->left.get());
  auto from = lowerType(expr->left->resolvedType);
  auto to = lowerType(expr->resolvedType);
  return make_unique<HIRCastExpr>(std::move(operand), from, to);
=======
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
>>>>>>> dd7486765f77f2c69a9a91ef8a8c9197d27e5710
}
