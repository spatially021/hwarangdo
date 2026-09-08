#include "hrd/AST/Decl.h"
#include "hrd/AST/Expr.h"
#include "hrd/IR/HIR/HIRBuilder.h"
#include "hrd/IR/HIR/HIRDecl.h"
#include "hrd/IR/HIR/HIRExpr.h"
#include "hrd/IR/HIR/HIRStmt.h"
#include "hrd/IR/HIR/HIRSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/Symbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/util/Error.h"
#include <cassert>
#include <cstddef>
#include <memory>
#include <utility>
#include <variant>
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
  MethodSymbol *methodDecl = nullptr;
  if (auto method = get_if<MethodSymbol *>(&expr->resolved)) {

    methodDecl = *method;
  } else {
    Error::internal(expr->span, "method call but not methodSymbol");
  }

  vector<unique_ptr<HIRExpr>> args;

  for (size_t i = 0; i < expr->arguments.size(); ++i) {
    auto symbol = get_if<MethodSymbol *>(&expr->resolved);
    if (symbol == nullptr) {
      Error::internal(expr->span, "illegal symbol kind");
    }

    args.push_back(
        lowerCallArg(expr->arguments[i].get(), (*symbol)->params[i]));
  }

  return make_unique<HIRMethodCallExpr>(expr->span, std::move(receiver),
                                        methodDecl, std::move(args),
                                        expr->resolvedType);
}

std::unique_ptr<HIRExpr> HIRBuilder::lowerCall(CallExpr *expr) {
  assert(expr);

  std::unique_ptr<HIRValueExpr> receiver = nullptr;
  if (!expr->isStatic) {
    receiver = lowerReceiver(expr->receiver.get());
    if (receiver == nullptr) {
      Error::internal(expr->span, "fail to lower receiver");
    }
  }

  MethodSymbol *methodDecl = nullptr;
  if (auto method = get_if<MethodSymbol *>(&expr->resolved)) {

    methodDecl = *method;
  } else {
    Error::internal(expr->span, "method call but not methodSymbol");
  }

  vector<unique_ptr<HIRExpr>> args;
  for (size_t i = 0; i < expr->arguments.size(); ++i) {
    auto symbol = get_if<MethodSymbol *>(&expr->resolved);
    if (symbol == nullptr) {
      Error::internal(expr->span, "illegal symbol kind");
    }

    args.push_back(
        lowerCallArg(expr->arguments[i].get(), (*symbol)->params[i]));
  }

  if (expr->resolvedType == nullptr) {
    Error::internal(expr->span, "resolvedType is nullptr");
  }

  auto rType = expr->resolvedType;

  return make_unique<HIRMethodCallExpr>(expr->span, std::move(receiver),
                                        methodDecl, std::move(args), rType);
}

unique_ptr<HIRValueExpr> HIRBuilder::lowerCallArg(Expr *expr,
                                                  ParamSymbol *param) {

  assert(expr);

  if (dynamic_cast<DefaultValueExpr *>(expr)) {
    if (get_if<std::monostate>(&param->defaultValue)) {
      Error::internal(expr->span, "parameter has no default value");
    }
    if (auto lit = get_if<LiteralExpr *>(&param->defaultValue)) {
      return lowerValue(*lit);
    }
    if (auto call = get_if<CallExpr *>(&param->defaultValue)) {
      return lowerValue(*call);
    }
    Error::internal(expr->span, "unknown default value");
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
  auto type = expr->resolvedType;

  return make_unique<HIRTernaryExpr>(expr->span, std::move(cond),
                                     std::move(then), std::move(else_), type);
}

unique_ptr<HIRExpr> HIRBuilder::lowerCast(CastExpr *expr) {
  auto operand = lowerValue(expr->left.get());
  auto from = expr->left->resolvedType;
  auto to = expr->resolvedType;
  return make_unique<HIRCastExpr>(expr->span, std::move(operand), from, to);
}

unique_ptr<HIRExpr> HIRBuilder::lowerMatch(MatchExpr *expr) {
  unique_ptr<HIRValueExpr> cond = lowerValue(expr->value.get());
  vector<unique_ptr<HIRCase>> cases;
  for (auto &c : expr->cases) {
    cases.push_back(lowerCase(c.get()));
  }
  TypeSymbol *type = expr->resolvedType;
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
  default:
    Error::internal(expr->span, "unknown storage kind");
  }

  // typeSymbol->hirEntityType
  // nullptr not allowed
  TypeSymbol *entity = expr->spawnType->resolved;

  // make handle with entity and stoagekind
  // nullptr not allowed
  TypeSymbol *handle = expr->resolvedType;

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
    auto [result, method] = lookupInit(decl, expr->resolvedInit);
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
  default:
    Error::internal(expr->span, "unknown storage kind");
  };

  TypeSymbol *entity = expr->resolvedType;
  auto handle = lowerValue(expr->target.get());
  TypeSymbol *observer = entity;

  return make_unique<HIRViewExpr>(expr->span, observer, storageKind,
                                  std::move(handle), entity);
}

unique_ptr<HIRExpr> HIRBuilder::lowerInitCall(CallExpr *expr) {
  assert(expr);
  if (!holds_alternative<std::monostate>(expr->resolved)) {
    MethodSymbol *methodDecl = nullptr;

    if (auto method = get_if<MethodSymbol *>(&expr->resolved)) {
      methodDecl = *method;
    } else {
      Error::internal(expr->span, "method call but not methodSymbol");
    }

    vector<unique_ptr<HIRExpr>> args;
    for (size_t i = 0; i < expr->arguments.size(); ++i) {
      auto symbol = get_if<MethodSymbol *>(&expr->resolved);
      if (symbol == nullptr) {
        Error::internal(expr->span, "illegal symbol kind");
      }

      args.push_back(
          lowerCallArg(expr->arguments[i].get(), (*symbol)->params[i]));
    }

    if (expr->resolvedType == nullptr) {
      Error::internal(expr->span, "resolvedType is nullptr");
    }

    return make_unique<HIRStructInitExpr>(
        expr->span, methodDecl, std::move(args), expr->resolvedType, true);
  }
  vector<unique_ptr<HIRExpr>> args;

  auto rType = expr->resolvedType;

  return make_unique<HIRStructInitExpr>(expr->span, nullptr, std::move(args),
                                        rType, true);
}

unique_ptr<HIRExpr> HIRBuilder::lowerLiteral(LiteralExpr *expr) {
  if (!expr->resolvedType) {
    Error::internal(expr->span, "literal has no resolved type");
  }

  auto *ty = expr->resolvedType;

  return std::make_unique<HIRLiteralExpr>(expr->span, ty, expr->resolvedLit);
}

unique_ptr<HIRExpr> HIRBuilder::lowerArrayLiteral(ArrayLiteralExpr *expr) {
  if (expr->resolvedType == nullptr) {
    Error::internal(expr->span, "array literal has no resolved type");
  }
  if (expr->elementType == nullptr) {
    Error::internal(expr->span, "array literal's element type is nullptr");
  }

  auto *ty = expr->resolvedType;
  vector<unique_ptr<HIRExpr>> elements;
  for (auto e : expr->elements) {
    elements.push_back(lowerExpr(e.get()));
  }
  return std::make_unique<HIRArrayLiteralExpr>(
      expr->span, ty, expr->elementType, std::move(elements));
}

unique_ptr<HIRExpr> HIRBuilder::lowerRuntime(CallExpr *expr) {
  if (expr->resolvedType == nullptr) {
    Error::internal(expr->span, "runtimeCall has no resolved type");
  }

  auto ty = expr->resolvedType;
  auto runtime = get_if<RuntimeSymbol *>(&expr->resolved);
  if (runtime == nullptr) {
    Error::internal(expr->span, "fail to get runtimeSymbol");
  }

  vector<unique_ptr<HIRExpr>> args;
  for (auto &a : expr->arguments) {

    args.push_back(lowerValue(a.get()));
  }
  return make_unique<HIRRuntimeCall>(expr->span, *runtime, std::move(args), ty);
}
