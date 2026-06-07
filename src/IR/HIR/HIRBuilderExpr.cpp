#include "AST/Decl.h"
#include "AST/Expr.h"
#include "IR/HIR/HIRBuilder.h"
#include "IR/HIR/HIRDecl.h"
#include "IR/HIR/HIRExpr.h"
#include "IR/HIR/HIRHelper.h"
#include "IR/HIR/HIRStmt.h"
#include "IR/HIR/HIRSymbol.h"
#include "IR/HIR/HIRType.h"
#include "SemanticAnalyzer/symbol/MethodSymbol.h"
#include "SemanticAnalyzer/symbol/Symbol.h"
#include "util/Error.h"
#include <cassert>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

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
  auto it = program->typeCache.find(expr->resolvedType);

  if (it == program->typeCache.end()) {
    Error::internal("fail to find return type");
  }

  return make_unique<HIRMethodCallExpr>(
      expr->span, std::move(receiver), methodDecl, std::move(args), it->second);
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

  auto rType = it->second;

  return make_unique<HIRMethodCallExpr>(expr->span, std::move(receiver),
                                        methodDecl, std::move(args), rType);
}

unique_ptr<HIRValueExpr> HIRBuilder::lowerCallArg(Expr *expr, Param *param) {

  assert(expr);

  if (dynamic_cast<DefaultValueExpr *>(expr)) {
    if (!param->defaultValue.has_value()) {
      Error::internal(expr->span, "parameter has no default value");
    }
    return lowerValue(param->defaultValue.value().get());
  }

  return lowerValue(expr);
}

unique_ptr<HIRExpr> HIRBuilder::lowerTernary(TernaryExpr *expr) {
  // expr->hirValue
  // nullptr not allowed
  auto cond = lowerValue(expr->conditon.get());
  auto then = lowerValue(expr->then.get());
  auto else_ = lowerValue(expr->else_.get());

  // typeSymbol->hirType
  // nullptr not allowed
  auto type = HIRHelper::lowerType(program, source, expr->resolvedType);

  return make_unique<HIRTernaryExpr>(expr->span, std::move(cond),
                                     std::move(then), std::move(else_), type);
}

unique_ptr<HIRExpr> HIRBuilder::lowerCast(CastExpr *expr) {
  auto operand = lowerValue(expr->left.get());
  auto from = HIRHelper::lowerType(program, source, expr->left->resolvedType);
  auto to = HIRHelper::lowerType(program, source, expr->resolvedType);
  return make_unique<HIRCastExpr>(expr->span, std::move(operand), from, to);
}

unique_ptr<HIRExpr> HIRBuilder::lowerMatch(MatchExpr *expr) {
  unique_ptr<HIRValueExpr> cond = lowerValue(expr->value.get());
  vector<unique_ptr<HIRCase>> cases;
  for (auto &c : expr->cases) {
    cases.push_back(lowerCase(c.get()));
  }
  HIRType *type = HIRHelper::lowerType(program, source, expr->resolvedType);
  return make_unique<HIRMatchExpr>(expr->span, type, std::move(cond),
                                   std::move(cases));
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
  HIREntityType *entity =
      HIRHelper::lowerEntityType(program, source, expr->spawnType->resolved);

  // make handle with entity and stoagekind
  // nullptr not allowed
  HIRHandleType *handle =
      HIRHelper::getOrCreateHandleType(program, source, entity, storageKind);

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

  return make_unique<HIRSpawnExpr>(expr->span, handle, storageKind, entity,
                                   init, std::move(args));
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

  HIREntityType *entity =
      HIRHelper::lowerEntityType(program, source, expr->resolvedType);
  auto handle = lowerValue(expr->target.get());
  HIRObserverType *observer = getOrCreateObserverType(entity, storageKind);

  return make_unique<HIRViewExpr>(expr->span, observer, storageKind,
                                  std::move(handle), entity);
}

unique_ptr<HIRExpr> HIRBuilder::lowerInitCall(CallExpr *expr) {
  assert(expr);
  HIRBlockStmt *defaultInit = nullptr;
  auto type = table->getType(expr->methodName);
  if (type == nullptr) {
    Error::internal(expr->span, "fail to get typeSymbol");
  }
  auto typeIt = program->typeDeclMap.find(type);
  if (typeIt == program->typeDeclMap.end()) {
    Error::internal(expr->span, "fail to get receiver's typeDecl");
  }
  defaultInit = typeIt->second->defaultInitBlock.get();

  if (expr->resolved != nullptr) {
    HIRMethodDecl *methodDecl = nullptr;

    if (auto method = dynamic_cast<MethodSymbol *>(expr->resolved)) {

      auto it = typeIt->second->initMap.find(method);
      if (it == currentType->initMap.end()) {
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

    auto rType = it->second;

    return make_unique<HIRStructInitExpr>(expr->span, methodDecl,
                                          std::move(args), rType, defaultInit);
  }
  vector<unique_ptr<HIRExpr>> args;

  return make_unique<HIRStructInitExpr>(expr->span, nullptr, std::move(args),
                                        program->voidType, defaultInit, true);
}

unique_ptr<HIRExpr> HIRBuilder::lowerLiteral(LiteralExpr *expr) {
  if (!expr->resolvedType) {
    Error::internal(expr->span, "literal has no resolved type");
  }

  auto *ty = HIRHelper::lowerType(program, source, expr->resolvedType);

  return std::make_unique<HIRLiteralExpr>(expr->span, ty, expr->resolvedLit);
}