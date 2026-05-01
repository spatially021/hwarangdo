#include "AST/Decl.h"
#include "AST/Expr.h"
#include "IR/HIR/HIRBuilder.h"
#include "IR/HIR/HIRDecl.h"
#include "IR/HIR/HIRExpr.h"
#include "IR/HIR/HIRSymbol.h"
#include "IR/HIR/HIRType.h"
#include "SemanticAnalyzer/symbol/MethodSymbol.h"
#include "SemanticAnalyzer/symbol/Symbol.h"
#include "Token.h"
#include "enums/Operator.h"
#include "util/Error.h"
#include <cassert>
#include <cstddef>
#include <memory>
#include <utility>

unique_ptr<HIRExpr> HIRBuilder::lowerExpr(Expr *expr) {
  if (expr == nullptr) {
    Error::internal("lowering expr is nullptr");
  }
  exprResult.reset();
  expr->accept(this);
  if (exprResult == nullptr) {
    Error::internal(expr->span, "expr result nullptr");
  }
  return std::move(exprResult);
}

unique_ptr<HIRExpr> HIRBuilder::lowerImplictCall(CallExpr *expr) {
  auto receiver = lowerImplictSelf();
  if (receiver == nullptr) {
    Error::internal(expr->span, "fail to lower implict self");
  }
  HIRMethodDecl *methodDecl = nullptr;
  if (auto method = dynamic_cast<MethodSymbol *>(expr->resolved)) {
    auto it = currentType->methodMap.find(method);
    if (it == currentType->methodMap.end()) {
      Error::internal("fail to find methodDecl");
    }
    methodDecl = it->second;
  } else {
    Error::internal(expr->span, "method call but not methodSymbol");
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

  return make_unique<HIRMethodCallExpr>(
      std::move(receiver), methodDecl, std::move(args), it->second, expr->span);
}

std::unique_ptr<HIRExpr> HIRBuilder::lowerCall(CallExpr *expr) {
  assert(expr);
  std::unique_ptr<HIRValueExpr> receiver = lowerReceiver(expr->receiver.get());
  if (receiver == nullptr) {
    Error::internal(expr->span, "fail to lower receiver");
  }
  HIRMethodDecl *methodDecl = nullptr;
  if (auto method = dynamic_cast<MethodSymbol *>(expr->resolved)) {
    auto typeIt = program->typeDeclMap.find(expr->receiver->resolvedType);
    if (typeIt == program->typeDeclMap.end()) {
      Error::internal(expr->span, "fail to get receiver's typeDecl");
    }
    auto it = typeIt->second->methodMap.find(method);
    if (it == currentType->methodMap.end()) {
      Error::internal(expr->span, "fail to find methodDecl");
    }
    methodDecl = it->second;
  } else {
    Error::internal(expr->span, "method call but not methodSymbol");
  }

  vector<unique_ptr<HIRExpr>> args;
  for (size_t i = 0; i < expr->arguments.size(); ++i) {
    auto decl = dynamic_cast<FuncDecl *>(
        dynamic_cast<MethodSymbol *>(expr->resolved)->decl);
    args.push_back(
        lowerCallArg(expr->arguments[i].get(), decl->params[i].get()));
  }

  if (expr->resolvedType == nullptr) {
    Error::internal(expr->span, "resolvedType is nullptr");
  }

  auto it = program->typeCache.find(expr->resolvedType);

  if (it == program->typeCache.end()) {
    Error::internal(expr->span,
                    "fail to find return type : " + expr->resolvedType->name);
  }
  return make_unique<HIRMethodCallExpr>(
      std::move(receiver), methodDecl, std::move(args), it->second, expr->span);
}

unique_ptr<HIRValueExpr> HIRBuilder::lowerCallArg(Expr *expr, Param *param) {
  if (dynamic_cast<DefaultValueExpr *>(expr)) {
    if (!param->defaultValue.has_value()) {
      Error::internal(expr->span, "parameter has no default value");
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
    Error::internal(expr->span, "lhs is not nameExpr");
  }
  unique_ptr<HIRValueExpr> rhs = lowerValue(expr->value.get());

  switch (expr->op.kind) {

  case TKind::PLUS_EQUAL:
    return make_unique<HIRCompoundAssignExpr>(std::move(lhs), std::move(rhs),
                                              Operator::ADD, expr->span);

  case TKind::MINUS_EQUAL:
    return make_unique<HIRCompoundAssignExpr>(std::move(lhs), std::move(rhs),
                                              Operator::SUB, expr->span);

  case TKind::STAR_EQUAL:
    return make_unique<HIRCompoundAssignExpr>(std::move(lhs), std::move(rhs),
                                              Operator::MUL, expr->span);

  case TKind::DOUBLE_STAR_EQUAL:
    return make_unique<HIRCompoundAssignExpr>(std::move(lhs), std::move(rhs),
                                              Operator::POW, expr->span);

  case TKind::SLASH_EQUAL:
    return make_unique<HIRCompoundAssignExpr>(std::move(lhs), std::move(rhs),
                                              Operator::SUB, expr->span);

  case TKind::PERCENT_EQUAL:
    return make_unique<HIRCompoundAssignExpr>(std::move(lhs), std::move(rhs),
                                              Operator::REM, expr->span);

  case TKind::CARET_EQUAL:
    return make_unique<HIRCompoundAssignExpr>(std::move(lhs), std::move(rhs),
                                              Operator::B_AND, expr->span);

  case TKind::AMPERSAND_EQAUL:
    return make_unique<HIRCompoundAssignExpr>(std::move(lhs), std::move(rhs),
                                              Operator::B_XOR, expr->span);

  case TKind::PIPE_EQUAL:
    return make_unique<HIRCompoundAssignExpr>(std::move(lhs), std::move(rhs),
                                              Operator::B_OR, expr->span);

  case TKind::DOUBLE_ANGLEBUCKET_EQAUL:
    return make_unique<HIRCompoundAssignExpr>(std::move(lhs), std::move(rhs),
                                              Operator::LSH, expr->span);

  case TKind::DOUBLE_RIGHT_ANGLE_BUCKET_EQUAL:
    return make_unique<HIRCompoundAssignExpr>(std::move(lhs), std::move(rhs),
                                              Operator::RSH, expr->span);

  case TKind::EQUAL:
    return make_unique<HIRAssignExpr>(std::move(lhs), std::move(rhs),
                                      expr->span);
  default:
    Error::internal(expr->span, "illegal operator kind");
    break;
  }

  // expr->hirvalueExpr
  // not allowed nullptr
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
                                     std::move(else_), type, expr->span);
}

unique_ptr<HIRExpr> HIRBuilder::lowerCast(CastExpr *expr) {
  auto operand = lowerValue(expr->left.get());
  auto from = lowerType(expr->left->resolvedType);
  auto to = lowerType(expr->resolvedType);
  return make_unique<HIRCastExpr>(std::move(operand), from, to, expr->span);
}

unique_ptr<HIRExpr> HIRBuilder::lowerMatch(MatchExpr *expr) {
  unique_ptr<HIRValueExpr> cond = lowerValue(expr->value.get());
  vector<unique_ptr<HIRCase>> cases;
  for (auto &c : expr->cases) {
    cases.push_back(lowerCase(c.get()));
  }
  HIRType *type = lowerType(expr->resolvedType);
  return make_unique<HIRMatchExpr>(type, std::move(cond), std::move(cases),
                                   expr->span);
}

unique_ptr<HIRExpr> HIRBuilder::lowerSpawn(SpawnExpr *expr) {

  auto storage = dynamic_cast<BuiltInNameExpr *>(expr->left.get());
  if (storage == nullptr) {
    Error::internal(expr->span, "iliegal astNode kind");
  }
  StorageKind storageKind;
  switch (storage->storageType) {
  case BuiltInNameExpr::StorageType::WORLD:
    storageKind = StorageKind::World;
    break;
  case BuiltInNameExpr::StorageType::ARENA:
    storageKind = StorageKind::Arena;
    break;
  default:
    Error::internal(expr->span, "unknown storage kind");
  }

  // typeSymbol->hirEntityType
  // nullptr not allowed
  HIREntityType *entity = lowerEntityType(expr->spawnType->resolved);

  // make handle with entity and stoagekind
  // nullptr not allowed
  HIRHandleType *handle = getOrCreateHandleType(entity, storageKind);

  vector<unique_ptr<HIRExpr>> args;
  for (auto &a : expr->args) {
    args.push_back(lowerExpr(a.get()));
  }

  HIRMethodDecl *init = nullptr;

  auto it = program->typeDeclMap.find(expr->spawnType->resolved);

  if (it == program->typeDeclMap.end()) {
    Error::internal(expr->span, "fail to find TypeDecl");
  }

  HIRTypeDecl *decl = it->second;

  if (expr->resolvedInit != nullptr) {
    auto [result, method] = lookupMethod(decl, expr->resolvedInit);
    if (result) {
      init = method;
    } else {
      Error::internal(expr->span,
                      "fail to find method : " + expr->resolvedInit->name);
    }
  }

  return make_unique<HIRSpawnExpr>(handle, storageKind, entity, init,
                                   std::move(args), expr->span);
}

unique_ptr<HIRExpr> HIRBuilder::lowerView(ViewExpr *expr) {
  auto storage = dynamic_cast<BuiltInNameExpr *>(expr->left.get());
  if (storage == nullptr) {
    Error::internal(expr->span, "iliegal astNode kind");
  }
  StorageKind storageKind;
  switch (storage->storageType) {
  case BuiltInNameExpr::StorageType::WORLD:
    storageKind = StorageKind::World;
    break;
  case BuiltInNameExpr::StorageType::ARENA:
    storageKind = StorageKind::Arena;
    break;
  default:
    Error::internal(expr->span, "unknown storage kind");
  };

  NameExpr *name = dynamic_cast<NameExpr *>(expr->target.get());

  if (name == nullptr) {
    Error::internal(expr->span, "illegal astNode type");
  }

  auto place = lowerPlace(name);

  if (place == nullptr) {
    Error::internal(expr->span, "view place is nullptr");
  }

  auto handleType = dynamic_cast<HIRHandleType *>(place->type);
  if (handleType == nullptr) {
    Error::internal(expr->span, "expect handle : " + place->type->name);
  }

  if (handleType->storage != storageKind) {
    Error::internal(expr->span, "handle storage kind mismatch");
  }

  HIREntityType *entity = handleType->entityType;

  unique_ptr<HIRValueExpr> handle = load(std::move(place));

  HIRObserverType *observer = getOrCreateObserverType(entity, storageKind);

  return make_unique<HIRViewExpr>(observer, storageKind, std::move(handle),
                                  entity, expr->span);
}