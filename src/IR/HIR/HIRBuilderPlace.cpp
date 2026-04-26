

#include "AST/Expr.h"
#include "IR/HIR/HIRBuilder.h"
#include "IR/HIR/HIRExpr.h"
#include "IR/HIR/HIRSymbol.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "SemanticAnalyzer/symbol/ValueSymbol.h"
#include "util/Error.h"
#include <memory>
#include <utility>

std::unique_ptr<HIRValueExpr> HIRBuilder::lowerVariantValue(CallExpr *expr) {
  auto symbol = dynamic_cast<EnumVariantSymbol *>(expr->resolved);
  if (symbol == nullptr) {
    Error::internal(
        expr->token,
        "in enum variant context but resolved is not EnumVariantSymbol");
  }

  auto [found, variant] = lookupVariant(symbol);
  if (!found || variant == nullptr) {
    Error::internal(expr->token, "failed to find HIR enum variant");
  }

  if (expr->receiver == nullptr || expr->resolvedType == nullptr) {
    Error::internal(expr->token, "enum variant receiver type is nullptr");
  }

  auto it = program->typeDeclMap.find(expr->resolvedType);
  if (it == program->typeDeclMap.end() || it->second == nullptr) {
    Error::internal(expr->token,
                    "failed to find enum typeDecl for variant receiver");
  }

  auto owner = it->second;

  if (variant->owner == nullptr) {
    Error::internal(expr->token, "enum variant owner is nullptr");
  }

  if (variant->owner != owner) {
    Error::internal(expr->token, "enum variant owner mismatch");
  }

  std::unique_ptr<HIRExpr> payload = nullptr;

  if (variant->payloadType == nullptr) {
    if (!expr->arguments.empty()) {
      Error::internal(expr->token, "unit variant cannot have arguments");
    }
  } else {
    if (expr->arguments.size() != 1) {
      Error::internal(expr->token,
                      "payload variant requires exactly one argument");
    }
    payload = lowerExpr(expr->arguments[0].get());
    if (payload == nullptr) {
      Error::internal(expr->token, "failed to lower enum variant payload");
    }
  }

  return std::make_unique<HIRVaraintValueExpr>(owner->type, variant,
                                               std::move(payload));
}

std::unique_ptr<HIRValueExpr> HIRBuilder::lowerVariantValue(MemberExpr *expr) {
  auto symbol = dynamic_cast<EnumVariantSymbol *>(expr->resolved);
  if (symbol == nullptr) {
    Error::internal(expr->token, "unmatched resolved");
  }

  auto [found, variant] = lookupVariant(symbol);
  if (!found || variant == nullptr) {
    Error::internal(expr->token, "failed to find HIR enum variant");
  }

  if (variant->payloadType != nullptr) {
    Error::internal(expr->token, "unit variant cannot be used with payload");
  }

  auto it = program->typeDeclMap.find(expr->resolvedType);
  if (it == program->typeDeclMap.end()) {
    Error::diagnostic(expr->token, "unknown enum variant");
  }

  auto owner = it->second;

  if (variant->owner == nullptr) {
    Error::internal(expr->token, "enum variant owenr is nullptr");
  }

  if (variant->owner != owner) {
    Error::internal(expr->token, "mismatched variant owner");
  }

  return make_unique<HIRVaraintValueExpr>(owner->type, variant);
}

unique_ptr<HIRPlaceExpr> HIRBuilder::lowerPlace(NameExpr *expr) {
  if (expr == nullptr) {
    Error::internal("nameExpr is nullptr");
  }
  if (expr->resolved == nullptr) {
    Error::internal(expr->token, "unresolved symbol");
  }

  if (expr->resolved->type != Symbol::SymbolType::VALUE) {
    Error::internal("iliegal symbol kind");
  }

  auto value = dynamic_cast<ValueSymbol *>(expr->resolved);

  if (auto [cond, result] = lookupLocal(value); cond) {
    return make_unique<HIRLocalPlaceExpr>(result);
  }
  if (auto [cond, result] = lookupParam(value); cond) {
    return make_unique<HIRParamPlaceExpr>(result);
  }
  if (auto [cond, result] = lookupField(value); cond) {

    return make_unique<HIRFieldPlaceExpr>(lowerImplictSelf(), result);
  }
  Error::internal(expr->token, "unregisitered value");
}

std::unique_ptr<HIRFieldPlaceExpr> HIRBuilder::lowerMember(MemberExpr *expr) {
  std::unique_ptr<HIRValueExpr> receiver = lowerReceiver(expr->object.get());
  if (receiver == nullptr) {
    Error::internal(expr->token, "receiver is nullptr");
  }
  if (expr->resolved == nullptr) {
    Error::internal(expr->token, "unresolved member symbol");
  }

  auto value = dynamic_cast<ValueSymbol *>(expr->resolved);
  if (value == nullptr) {
    Error::internal(expr->token, "member field resolved is not ValueSymbol");
  }

  auto it = program->typeDeclMap.find(expr->object->resolvedType);
  if (it == program->typeDeclMap.end()) {
    Error::internal(expr->token, "unknown type");
  }

  if (dynamic_cast<HIRRootExpr *>(receiver.get())) {
    auto rIt = program->rootMap.find(value);
    if (rIt != program->rootMap.end()) {
      return make_unique<HIRFieldPlaceExpr>(std::move(receiver), rIt->second);
    }
  }

  if (auto [cond, result] = lookupField(it->second, value); cond) {
    return make_unique<HIRFieldPlaceExpr>(std::move(receiver), result);
  }

  Error::internal(expr->token, "fail to lower field");
}

std::unique_ptr<HIRPlaceExpr>
HIRBuilder::lowerArrayAccess(ArrayAccessExpr *expr) {
  auto type = dynamic_cast<ArrayTypeSymbol *>(expr->object->resolvedType);
  if (type == nullptr) {
    Error::internal(expr->token, "expected array type ");
  }

  // Expr -> hirValueExpr
  // nullptr아님을 보장
  unique_ptr<HIRValueExpr> object = lowerValue(expr->object.get());
  unique_ptr<HIRValueExpr> index = lowerValue(expr->index.get());

  // TypeSymbol -> hirType
  // nullptr 아님을 보장
  auto elementType = lowerType(expr->object->resolvedType);

  return make_unique<HIRArrayAccessPlaceExpr>(std::move(object),
                                              std::move(index), elementType);
}