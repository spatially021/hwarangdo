

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
        expr->span,
        "in enum variant context but resolved is not EnumVariantSymbol");
  }

  auto [found, variant] = lookupVariant(symbol);
  if (!found || variant == nullptr) {
    Error::internal(expr->span, "failed to find HIR enum variant");
  }

  if (expr->receiver == nullptr || expr->resolvedType == nullptr) {
    Error::internal(expr->span, "enum variant receiver type is nullptr");
  }

  auto it = program->typeDeclMap.find(expr->resolvedType);
  if (it == program->typeDeclMap.end() || it->second == nullptr) {
    Error::internal(expr->span,
                    "failed to find enum typeDecl for variant receiver");
  }

  auto owner = it->second;

  if (variant->owner == nullptr) {
    Error::internal(expr->span, "enum variant owner is nullptr");
  }

  if (variant->owner != owner) {
    Error::internal(expr->span, "enum variant owner mismatch");
  }

  std::unique_ptr<HIRExpr> payload = nullptr;

  if (variant->payloadType == nullptr) {
    if (!expr->arguments.empty()) {
      Error::internal(expr->span, "unit variant cannot have arguments");
    }
  } else {
    if (expr->arguments.size() != 1) {
      Error::internal(expr->span,
                      "payload variant requires exactly one argument");
    }
    payload = lowerExpr(expr->arguments[0].get());
    if (payload == nullptr) {
      Error::internal(expr->span, "failed to lower enum variant payload");
    }
  }

  return std::make_unique<HIRVaraintValueExpr>(expr->span, owner->type, variant,
                                               std::move(payload));
}

std::unique_ptr<HIRValueExpr> HIRBuilder::lowerVariantValue(MemberExpr *expr) {
  auto symbol = dynamic_cast<EnumVariantSymbol *>(expr->resolved);
  if (symbol == nullptr) {
    Error::internal(expr->span, "unmatched resolved");
  }

  auto [found, variant] = lookupVariant(symbol);
  if (!found || variant == nullptr) {
    Error::internal(expr->span, "failed to find HIR enum variant");
  }

  if (variant->payloadType != nullptr) {
    Error::internal(expr->span, "unit variant cannot be used with payload");
  }

  auto it = program->typeDeclMap.find(expr->resolvedType);
  if (it == program->typeDeclMap.end()) {
    Error::diagnostic(expr->span, "unknown enum variant");
  }

  auto owner = it->second;

  if (variant->owner == nullptr) {
    Error::internal(expr->span, "enum variant owenr is nullptr");
  }

  if (variant->owner != owner) {
    Error::internal(expr->span, "mismatched variant owner");
  }

  return make_unique<HIRVaraintValueExpr>(expr->span, owner->type, variant,
                                          nullptr);
}

unique_ptr<HIRPlaceExpr> HIRBuilder::lowerPlace(NameExpr *expr) {
  if (expr == nullptr) {
    Error::internal("nameExpr is nullptr");
  }
  if (expr->resolved == nullptr) {
    Error::internal(expr->span, "unresolved symbol");
  }

  if (expr->resolved->type != Symbol::SymbolType::VALUE) {
    Error::internal("iliegal symbol kind");
  }

  auto value = dynamic_cast<ValueSymbol *>(expr->resolved);
  if (value == nullptr) {
    Error::internal(expr->span, "valueSymbol is nullptr");
  }

  if (auto [cond, result] = lookupLocal(value); cond) {
    return make_unique<HIRLocalPlaceExpr>(expr->span, result);
  }
  if (auto [cond, result] = lookupParam(value); cond) {
    return make_unique<HIRParamPlaceExpr>(expr->span, result);
  }
  if (auto [cond, result] = lookupField(value); cond) {
    return make_unique<HIRFieldPlaceExpr>(expr->span, lowerImplictSelf(),
                                          result);
  }
  Error::internal(expr->span, "unregisitered value : " + expr->name);
}

std::unique_ptr<HIRFieldPlaceExpr> HIRBuilder::lowerMember(MemberExpr *expr) {
  std::unique_ptr<HIRValueExpr> receiver = lowerReceiver(expr->object.get());
  if (receiver == nullptr) {
    Error::internal(expr->span, "receiver is nullptr");
  }
  if (expr->resolved == nullptr) {
    Error::internal(expr->span, "unresolved member symbol");
  }

  auto value = dynamic_cast<ValueSymbol *>(expr->resolved);
  if (value == nullptr) {
    Error::internal(expr->span, "member field resolved is not ValueSymbol");
  }

  auto it = program->typeDeclMap.find(expr->object->resolvedType);
  if (it == program->typeDeclMap.end()) {
    Error::internal(expr->span, "unknown type");
  }

  if (dynamic_cast<HIRRootExpr *>(receiver.get())) {
    auto rIt = program->rootMap.find(value);
    if (rIt != program->rootMap.end()) {
      return make_unique<HIRFieldPlaceExpr>(expr->span, std::move(receiver),
                                            rIt->second);
    }
  }

  if (auto [cond, result] = lookupField(it->second, value); cond) {
    return make_unique<HIRFieldPlaceExpr>(expr->span, std::move(receiver),
                                          result);
  }

  Error::internal(expr->span, "fail to lower field");
}

std::unique_ptr<HIRPlaceExpr>
HIRBuilder::lowerArrayAccess(ArrayAccessExpr *expr) {

  auto type = dynamic_cast<ArrayTypeSymbol *>(expr->object->resolvedType);
  if (type == nullptr) {
    Error::internal(expr->span, "expected array type ");
  }

  // Expr -> hirValueExpr
  // nullptr아님을 보장
  unique_ptr<HIRValueExpr> object = lowerValue(expr->object.get());
  unique_ptr<HIRValueExpr> index = lowerValue(expr->index.get());

  // TypeSymbol -> hirType
  // nullptr 아님을 보장
  auto elementType = lowerType(expr->resolvedType);

  return make_unique<HIRArrayAccessPlaceExpr>(expr->span, std::move(object),
                                              std::move(index), elementType);
}