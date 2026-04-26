#include "AST/Decl.h"
#include "AST/Expr.h"
#include "IR/HIR/HIRBuilder.h"
#include "IR/HIR/HIRDecl.h"
#include "IR/HIR/HIRExpr.h"
#include "IR/HIR/HIRSymbol.h"
#include "IR/HIR/HIRType.h"
#include "SemanticAnalyzer/symbol/MethodSymbol.h"
#include "SemanticAnalyzer/symbol/Symbol.h"
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
    Error::internal(expr->token, "expr result nullptr : " + expr->token.text);
  }
  return std::move(exprResult);
}

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
      Error::internal(expr->token, "fail to get receiver's typeDecl");
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

  if (expr->resolvedType == nullptr) {
    Error::internal(expr->token, "resolvedType is nullptr");
  }

  auto it = program->typeCache.find(expr->resolvedType);

  if (it == program->typeCache.end()) {
    Error::internal(expr->token,
                    "fail to find return type : " + expr->resolvedType->name);
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
}

unique_ptr<HIRExpr> HIRBuilder::lowerMatch(MatchExpr *expr) {
  unique_ptr<HIRValueExpr> cond = lowerValue(expr->value.get());
  vector<unique_ptr<HIRCase>> cases;
  for (auto &c : expr->cases) {
    cases.push_back(lowerCase(c.get()));
  }
  HIRType *type = lowerType(expr->resolvedType);
  return make_unique<HIRMatchExpr>(type, std::move(cond), std::move(cases));
}

unique_ptr<HIRExpr> HIRBuilder::lowerSpawn(SpawnExpr *expr) {

  auto storage = dynamic_cast<BuiltInNameExpr *>(expr->left.get());
  if (storage == nullptr) {
    Error::internal(expr->token, "iliegal astNode kind");
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
    Error::internal(expr->token, "unknown storage kind");
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
    Error::internal(expr->token,
                    "fail to find TypeDecl : " + expr->spawnType->token.text);
  }

  HIRTypeDecl *decl = it->second;

  if (expr->resolvedInit != nullptr) {
    auto [result, method] = lookupMethod(decl, expr->resolvedInit);
    if (result) {
      init = method;
    } else {
      Error::internal(expr->token,
                      "fail to find method : " + expr->resolvedInit->name);
    }
  }

  return make_unique<HIRSpawnExpr>(handle, storageKind, entity, init,
                                   std::move(args));
}

unique_ptr<HIRExpr> HIRBuilder::lowerView(ViewExpr *expr) {
  auto storage = dynamic_cast<BuiltInNameExpr *>(expr->left.get());
  if (storage == nullptr) {
    Error::internal(expr->token, "iliegal astNode kind");
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
    Error::internal(expr->token, "unknown storage kind");
  };

  NameExpr *name = dynamic_cast<NameExpr *>(expr->target.get());

  if (name == nullptr) {
    Error::internal(expr->token,
                    "illegal astNode type : " + expr->target->token.text);
  }

  auto place = lowerPlace(name);

  if (place == nullptr) {
    Error::internal(expr->token, "view place is nullptr");
  }

  auto handleType = dynamic_cast<HIRHandleType *>(place->type);
  if (handleType == nullptr) {
    Error::internal(expr->token, "expect handle : " + place->type->name);
  }

  if (handleType->storage != storageKind) {
    Error::internal(expr->token, "handle storage kind mismatch");
  }

  HIREntityType *entity = handleType->entityType;

  unique_ptr<HIRValueExpr> handle = load(std::move(place));

  HIRObserverType *observer = getOrCreateObserverType(entity, storageKind);

  return make_unique<HIRViewExpr>(observer, storageKind, std::move(handle),
                                  entity);
}