#include "hrd/AST/ASTNode.h"
#include "hrd/AST/Decl.h"
#include "hrd/AST/Expr.h"
#include "hrd/AST/Stmt.h"
#include "hrd/SemanticAnalyzer/ResolvedLit.h"
#include "hrd/SemanticAnalyzer/Resolver.h"
#include "hrd/SemanticAnalyzer/Scope.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/StorageSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/Token.h"
#include "hrd/util/Error.h"
#include "hrd/util/TypeResolver.h"
#include "hrd/util/diagnostic/Diagnostic.h"
#include <cassert>
#include <memory>
#include <string>
#include <utility>

void Resolver::visit(LiteralExpr *expr) {
  ResolvedLit r;

  switch (expr->token.kind) {
  case TKind::LIT_INT:
    r = TypeResolver::resolveLitInt(expr, table);
    expr->resolvedType = r.type;
    expr->resolvedLit = r;
    break;

  case TKind::LIT_FLOAT:
    r = resolveLitFloat(expr);
    expr->resolvedType = r.type;
    expr->resolvedLit = r;
    break;

  case TKind::FIXED:
    expr->resolvedType = table.getType("fi16");
    break;

  case TKind::LIT_CHARACTER:
    r = resolveChar(expr);
    expr->resolvedType = r.type;
    expr->resolvedLit = r;
    break;

  case TKind::LIT_STRING:
    r = resolveString(expr);
    expr->resolvedType = r.type;
    expr->resolvedLit = r;
    break;

  case TKind::LIT_BOOL:
    expr->resolvedType = table.getBool();
    r.type = table.getBool();
    r.value = expr->value == "true";
    expr->resolvedLit = r;
    break;

  default:
    Error::internal(expr->token, "unknown literal type");
  }
}

void Resolver::visit(NameExpr *expr) {
  auto *symbol = resolveValue(expr->name);

  if (!symbol) {
    auto *type = table.getType(expr->name);

    if (type) {
      if (type->kind == TypeSymbol::TypeKind::ENUM) {
        expr->resolvedType = type;
        expr->resolved = type;
        return;
      }

      // TODO: 추후 static 메서드 추가 시 타입 이름 식의 허용 범위 확장 필요.
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S072);
      dia.labels = {
          {expr->span, "type '" + expr->name + "' cannot be used as a value",
           true},
      };
      dia.notes = {
          "only enum type names can currently appear in value expressions",
      };
      dia.helps = {
          "use a variable name or an enum type name",
      };
      engine.emit(dia);
      recover.recover();
    }

    if (currentType == nullptr) {
      Error::internal("currentType is nullptr");
    }

    if (currentType->baseName.has_value() && currentType->base) {
      symbol = lookLocalValue(expr->name, currentType->base->memberScope);
    }

    if (!symbol) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S073);
      dia.labels = {
          {expr->span,
           "name '" + expr->name + "' is not declared in this scope", true},
      };
      dia.helps = {
          "declare '" + expr->name + "' before using it",
      };
      engine.emit(dia);
      recover.recover();
    }
  }

  expr->resolved = symbol;
  expr->resolvedType = symbol->typeSymbol;

  if (!expr->resolvedType) {
    Error::internal(expr->span, "type symbol is nullptr");
  }

  if (dynamic_cast<HandleSymbol *>(expr->resolvedType)) {
    Error::internal(expr->span,
                    "non-generic HandleSymbol reached value resolution");
  }
}

void Resolver::visit(AssignExpr *expr) {
  expr->target->accept(this);
  expr->value->accept(this);

  if (!isAssignable(expr->target->resolvedType, expr->value->resolvedType)) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S074);
    dia.labels = {
        {expr->target->span,
         "target has type '" + expr->target->resolvedType->name + "'", true},
        {expr->value->span,
         "assigned value has type '" + expr->value->resolvedType->name + "'",
         false},
    };
    dia.notes = {
        "the assigned value cannot be implicitly converted to the target type",
    };
    dia.helps = {
        "assign a value compatible with '" + expr->target->resolvedType->name +
            "'",
    };
    engine.emit(dia);
    recover.recover();
  }

  if (expr->target->resolvedType->kind == TypeSymbol::TypeKind::CLASS) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S075);
    dia.labels = {
        {expr->target->span, "this observer cannot be reassigned", true},
    };
    dia.notes = {
        "observer variables provide temporary access to an entity",
        "observer assignment and copy initialization are not allowed",
    };
    dia.helps = {
        "create a new observer with 'world.view' instead",
    };
    engine.emit(dia);
    recover.recover();
  }

  expr->resolvedType =
      implicitCasting(expr->target.get(), expr->value->resolvedType).first;
}

void Resolver::visit(MemberExpr *expr) {
  expr->object->accept(this);

  auto *symbol = dynamic_cast<TypeSymbol *>(expr->object->resolvedType);
  if (symbol == nullptr) {
    Error::internal(expr->span, "fail to cast symbol");
  }

  if (symbol->kind == TypeSymbol::TypeKind::ENUM) {
    expr->resolved = lookupEnumVariant(symbol, expr->member, expr->span);
    expr->resolvedType = symbol;
    return;
  }

  auto *objectType = expr->object->resolvedType;

  if (dynamic_cast<GenericSymbol *>(objectType)) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S030);
    dia.labels = {
        {expr->object->span,
         "handle value cannot be used for direct member access", true},
    };
    dia.notes = {
        "entity members can only be accessed through an observer",
    };
    dia.helps = {
        "create an observer with 'world.view' before accessing the member",
    };
    engine.emit(dia);
    recover.recover();
  }

  ValueSymbol *member = nullptr;

  for (auto *type = symbol; type != nullptr; type = type->base) {
    auto it = type->memberScope->value.find(expr->member);

    if (it != type->memberScope->value.end()) {
      member = it->second.get();
      break;
    }
  }

  if (member == nullptr) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S076);
    dia.labels = {
        {expr->span,
         "type '" + symbol->name + "' has no member named '" + expr->member +
             "'",
         true},
    };
    dia.helps = {
        "use a member declared by '" + symbol->name + "'",
    };
    engine.emit(dia);
    recover.recover();
  }

  switch (member->modifier) {
  case AModifier::PUBLIC:
    break;

  case AModifier::PROTECTED: {
    if (dynamic_cast<SelfExpr *>(expr->object.get()) ||
        dynamic_cast<ThisExpr *>(expr->object.get()) ||
        dynamic_cast<SuperExpr *>(expr->object.get())) {
      break;
    }

    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S019);
    dia.labels = {
        {expr->span,
         "protected member '" + expr->member +
             "' cannot be accessed through this expression",
         true},
    };
    dia.notes = {
        "protected members are only accessible through 'self', 'this', or "
        "'super'",
    };
    dia.helps = {
        "access this member from an allowed inheritance context",
    };
    engine.emit(dia);
    recover.recover();
    break;
  }

  case AModifier::PRIVATE: {
    if (dynamic_cast<SelfExpr *>(expr->object.get()) ||
        dynamic_cast<ThisExpr *>(expr->object.get())) {
      break;
    }

    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S020);
    dia.labels = {
        {expr->span,
         "private member '" + expr->member +
             "' cannot be accessed from this context",
         true},
    };
    dia.notes = {
        "private members are only accessible through 'self' or 'this' within "
        "the declaring type",
    };
    dia.helps = {
        "access this member from within its declaring type",
    };
    engine.emit(dia);
    recover.recover();
  }
  }

  expr->resolved = member;
  expr->resolvedType = member->typeSymbol;
}

void Resolver::visit(CastExpr *expr) {
  expr->left->accept(this);
  expr->type->accept(this);

  if (expr->type->resolved == nullptr) {
    Error::internal(expr->span, "type node is nullptr");
  }

  expr->resolvedType = expr->type->resolved;
}

void Resolver::visit(DefaultValueExpr *expr) {
  expr->resolvedType = table.getDefaultV();

  if (!expr->resolvedType) {
    Error::internal(expr->span, "resolved type is nullptr");
  }
}