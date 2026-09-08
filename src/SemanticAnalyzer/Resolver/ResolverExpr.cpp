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
#include "hrd/SourceSpan.h"
#include "hrd/Token.h"
#include "hrd/diagnostic/Diagnostic.h"
#include "hrd/util/Error.h"
#include "hrd/util/Helper.h"
#include "hrd/util/TypeResolver.h"
#include <cassert>
#include <llvm/ADT/APInt.h>
#include <memory>
#include <string>
#include <utility>

void Resolver::visit(LiteralExpr *expr) {
  ResolvedLit r;

  switch (expr->token.kind) {
  case TKind::LIT_INT:
    r = TypeResolver::resolveLitInt(expr, typeContext);
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
    expr->resolvedType = table.registry.getBool();
    r.type = table.registry.getBool();
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
      if (isa<EnumType>(type) || isa<ObjectType>(type)) {
        expr->resolvedType = type;
        expr->resolved = type;
        return;
      }
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
    if (auto obj = dyn_cast<ObjectType>(currentType)) {
      if (obj->baseName.has_value() && obj->base) {
        symbol = lookLocalValue(expr->name, obj->base->memberScope);
      }
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
  if (auto arr = dynamic_cast<ArrayTypeSymbol *>(expr->target->resolvedType)) {
    if (arr->baseType == expr->value->resolvedType) {
      if (auto g = dynamic_cast<GenericSymbol *>(arr->baseType)) {
        if (g->origin->kind == TypeKind::HANDLE) {
          auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S132);
          dia.labels = {
              {expr->value->span,
               "this single spawn result is copied into every array element",
               true},
          };
          dia.notes = {
              "all array elements will contain Handles to the same entity",
          };
          dia.helps = {
              "spawn each element separately if the array should contain "
              "distinct entities",
          };
          engine.emit(dia);
        }
      }
      return;
    }
  }
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

  if (expr->target->resolvedType->kind == TypeKind::CLASS) {
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

  if (auto en = dyn_cast<EnumType>(symbol)) {
    expr->resolved = lookupEnumVariant(en, expr->member, expr->span);
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
  if (auto obj = dyn_cast<ObjectType>(symbol)) {
    for (auto *type = obj; type != nullptr; type = type->base) {
      auto it = type->memberScope->value.find(expr->member);

      if (it != type->memberScope->value.end()) {
        member = it->second.get();
        break;
      }
    }
  } else {
    Error::internal("illegal type kind");
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

void Resolver::visit(ArrayLiteralExpr *expr) {
  TypeSymbol *resolved = nullptr;
  SourceSpan expectSpan;

  if (expr->elements.empty()) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S131);
    dia.labels = {
        {expr->span, "this array literal has no elements", true},
    };
    dia.notes = {
        "the element type of an array literal is inferred from its elements",
    };
    dia.helps = {
        "add at least one element to the array literal",
    };
    engine.emit(dia);
    recover.recover();
  }

  for (auto &e : expr->elements) {
    e->accept(this);
    auto type = e->resolvedType;
    if (type == table.registry.getBuilt("void")) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S129);
      dia.labels = {
          {e->span, "this array element has type 'void'", true},
      };
      dia.notes = {
          "every element in an array literal must produce a storable value",
      };
      dia.helps = {
          "replace this expression with one that produces a non-void value",
      };
      engine.emit(dia);
      recover.recover();
    }

    if (resolved == nullptr) {
      resolved = type;
      expectSpan = e->span;
      continue;
    }

    if (resolved == type) {
      continue;
    }

    vector<TypeSymbol *> candidates = getPromotionCandidates(resolved, type);
    bool flag = false;
    for (auto *candidate : candidates) {
      auto leftResult = Helper::canImplicitlyConvert(resolved, candidate);
      auto rightResult = Helper::canImplicitlyConvert(type, candidate);
      if (leftResult.first && rightResult.first) {
        if (leftResult.second == CastingResultKind::PrecisionLoss ||
            rightResult.second == CastingResultKind::PrecisionLoss) {
          auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S069);
          dia.labels = {
              {e->span,
               "this operation implicitly converts between '" + resolved->name +
                   "' and '" + type->name + "'",
               true},
          };
          dia.notes = {
              "the implicit conversion may lose numeric precision",
          };
          dia.helps = {
              "use an explicit cast to acknowledge the possible precision loss",
          };
          engine.emit(dia);
        }

        resolved = candidate;
        flag = true;
        break;
      }
    }

    if (!flag) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S130);
      dia.labels = {
          {e->span, "this element has type '" + type->name + "'", true},
      };
      dia.notes = {
          "all elements in an array literal must resolve to the same type",
      };
      dia.helps = {
          "convert this element to the array's element type or use a separate "
          "array",
      };
      engine.emit(dia);
      recover.recover();
    }
  }
  expr->elementType = resolved;
  unsigned bits = 64;
  auto size = expr->elements.size();
  expr->resolvedType =
      table.registry.getOrCreateArray(resolved, llvm::APInt(bits, size));
}
