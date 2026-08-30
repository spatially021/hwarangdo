#include "hrd/AST/Expr.h"
#include "hrd/IR/HIR/HIRBuilder.h"
#include "hrd/IR/HIR/HIRExpr.h"
#include "hrd/IR/HIR/HIRSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/util/Error.h"
#include <memory>
#include <string>
#include <utility>
#include <variant>

std::unique_ptr<HIRValueExpr> HIRBuilder::lowerVariantValue(CallExpr *expr) {
  auto *symbol = get_if<EnumVariantSymbol *>(&expr->resolved);
  if (symbol == nullptr || *symbol == nullptr) {
    Error::internal(
        expr->span,
        "enum variant call resolved symbol is not EnumVariantSymbol");
  }

  if (expr->receiver == nullptr) {
    Error::internal(expr->span, "enum variant receiver is nullptr");
  }

  if (expr->resolvedType == nullptr) {
    Error::internal(expr->span, "enum variant resolved type is nullptr");
  }

  auto it = program->typeDeclMap.find(expr->resolvedType);
  if (it == program->typeDeclMap.end() || it->second == nullptr) {
    Error::internal(expr->span,
                    "failed to find enum type declaration for variant call");
  }

  auto *owner = it->second;

  std::unique_ptr<HIRExpr> payload = nullptr;

  return std::make_unique<HIRVariantValueExpr>(expr->span, owner->type, *symbol,
                                               std::move(payload));
}

std::unique_ptr<HIRValueExpr> HIRBuilder::lowerVariantValue(MemberExpr *expr) {
  auto *symbol = dynamic_cast<EnumVariantSymbol *>(expr->resolved);
  if (symbol == nullptr) {
    Error::internal(
        expr->span,
        "enum variant member resolved symbol is not EnumVariantSymbol");
  }

  if (expr->resolvedType == nullptr) {
    Error::internal(expr->span, "enum variant resolved type is nullptr");
  }

  auto it = program->typeDeclMap.find(expr->resolvedType);
  if (it == program->typeDeclMap.end() || it->second == nullptr) {
    Error::internal(expr->span,
                    "failed to find enum type declaration for variant member");
  }

  auto *owner = it->second;

  return std::make_unique<HIRVariantValueExpr>(expr->span, owner->type, symbol,
                                               nullptr);
}

std::unique_ptr<HIRPlaceExpr> HIRBuilder::lowerPlace(NameExpr *expr) {
  if (expr == nullptr) {
    Error::internal("NameExpr is nullptr");
  }

  if (expr->resolved == nullptr) {
    Error::internal(expr->span, "name expression has no resolved symbol");
  }

  if (expr->resolved->type != Symbol::SymbolType::VALUE) {
    Error::internal(expr->span,
                    "name expression resolved to a non-value symbol");
  }

  auto *value = dynamic_cast<ValueSymbol *>(expr->resolved);
  if (value == nullptr) {
    Error::internal(expr->span, "value symbol cannot be cast to ValueSymbol");
  }

  if (auto [found, local] = lookupLocal(value); found) {
    if (local == nullptr) {
      Error::internal(expr->span, "local lookup returned nullptr");
    }

    return std::make_unique<HIRLocalPlaceExpr>(expr->span, local);
  }

  if (auto [found, param] = lookupParam(value); found) {
    if (param == nullptr) {
      Error::internal(expr->span, "parameter lookup returned nullptr");
    }

    return std::make_unique<HIRParamPlaceExpr>(expr->span, param);
  }

  auto receiver = lowerImplictSelf();
  if (receiver == nullptr) {
    Error::internal(expr->span, "implicit self lowering returned nullptr");
  }

  return std::make_unique<HIRFieldPlaceExpr>(expr->span, std::move(receiver),
                                             value);

  Error::internal(expr->span, "unregistered value: " + expr->name);
}

std::unique_ptr<HIRFieldPlaceExpr> HIRBuilder::lowerMember(MemberExpr *expr) {
  auto receiver = lowerReceiver(expr->object.get());
  if (receiver == nullptr) {
    Error::internal(expr->object->span,
                    "member receiver lowering returned nullptr");
  }

  if (expr->resolved == nullptr) {
    Error::internal(expr->span, "member expression has no resolved symbol");
  }

  auto *value = dynamic_cast<ValueSymbol *>(expr->resolved);
  if (value == nullptr) {
    Error::internal(expr->span,
                    "member field resolved symbol is not ValueSymbol");
  }

  if (expr->object->resolvedType == nullptr) {
    Error::internal(expr->object->span,
                    "member receiver resolved type is nullptr");
  }

  return std::make_unique<HIRFieldPlaceExpr>(expr->span, std::move(receiver),
                                             value);

  Error::internal(expr->span, "failed to lower member field");
}

std::unique_ptr<HIRPlaceExpr>
HIRBuilder::lowerArrayAccess(ArrayAccessExpr *expr) {
  auto *arrayType = dynamic_cast<ArrayTypeSymbol *>(expr->object->resolvedType);
  if (arrayType == nullptr) {
    Error::internal(expr->object->span,
                    "array access object does not have an array type");
  }

  std::unique_ptr<HIRPlaceExpr> object = nullptr;

  if (auto *name = dynamic_cast<NameExpr *>(expr->object.get())) {
    object = lowerPlace(name);
  } else if (auto *array =
                 dynamic_cast<ArrayAccessExpr *>(expr->object.get())) {
    object = lowerArrayAccess(array);
  }

  if (object == nullptr) {
    Error::internal(expr->object->span,
                    "array access base is not a lowerable place expression");
  }

  auto index = lowerValue(expr->index.get());
  if (index == nullptr) {
    Error::internal(expr->index->span, "array index lowering returned nullptr");
  }

  if (auto lit = dynamic_cast<LiteralExpr *>(expr->index.get())) {
    auto in = lit->resolvedLit.asInt().value.getSExtValue();
    auto size = arrayType->sizeValue.getSExtValue();
    if (in < 0 || in >= size) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S133);
      dia.labels = {
          {expr->index->span,
           "'" + to_string(in) + "' is outside the valid array range", true},
      };
      dia.notes = {
          "array indices must be non-negative and smaller than the array "
          "length",
      };
      dia.notes = {
          "array indices must be non-negative and smaller than the array "
          "length",
      };
      engine.emit(dia);
      recover.recover();
    }
  }

  if (expr->resolvedType == nullptr) {
    Error::internal(expr->span, "array element resolved type is nullptr");
  }

  auto *elementType = expr->resolvedType;
  if (elementType == nullptr) {
    Error::internal(expr->span, "failed to lower array element type");
  }

  return std::make_unique<HIRArrayAccessPlaceExpr>(
      expr->span, std::move(object), std::move(index), elementType);
}