#include "hrd/AST/Decl.h"
#include "hrd/AST/Expr.h"
#include "hrd/AST/Stmt.h"
#include "hrd/SemanticAnalyzer/Resolver.h"
#include "hrd/SemanticAnalyzer/Scope.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/util/Error.h"
#include "hrd/util/Guard.h"
#include "hrd/util/diagnostic/Diagnostic.h"
#include <memory>

// Statement Resolver::visitor methods
void Resolver::visit(ExprStmt *stmt) { stmt->expr->accept(this); }

void Resolver::visit(BlockStmt *stmt) {
  ScopeGuard _(table, stmt->blockScope);

  for (auto &statement : stmt->statements) {
    statement->accept(this);
  }
}

void Resolver::visit(IfStmt *stmt) {
  stmt->condition->accept(this);

  if (!table.isBool(stmt->condition->resolvedType)) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S077);
    dia.labels = {
        {stmt->condition->span,
         "condition has type '" + stmt->condition->resolvedType->name + "'",
         true},
    };
    dia.notes = {
        "an if condition must have type 'bool'",
    };
    dia.helps = {
        "use a boolean expression as the condition",
    };
    engine.emit(dia);
    recover.recover();
  }

  stmt->thenBranch->accept(this);

  if (stmt->elseBranch != nullptr) {
    stmt->elseBranch->accept(this);
  }
}

void Resolver::visit(ForStmt *stmt) {
  stmt->initializer->accept(this);
  stmt->range->accept(this);

  if (auto *var = dynamic_cast<VarDecl *>(stmt->initializer->decl.get())) {
    if (!isAssignable(var->symbol->typeSymbol, stmt->range->resolvedType)) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S078);
      dia.labels = {
          {stmt->initializer->span,
           "loop variable has type '" + var->symbol->typeSymbol->name + "'",
           true},
          {stmt->range->span,
           "range produces values of type '" + stmt->range->resolvedType->name +
               "'",
           false},
      };
      dia.notes = {
          "range values must be assignable to the loop variable",
      };
      dia.helps = {
          "change the loop variable type to '" +
              stmt->range->resolvedType->name + "'",
      };
      engine.emit(dia);
      recover.recover();
    }
  } else {
    Error::internal(stmt->initializer->span, "initializer is not VarDecl");
  }

  ScopeGuard _(table, stmt->blockScope);
  stmt->body->accept(this);
}

void Resolver::visit(WhileStmt *stmt) {
  stmt->condition->accept(this);

  if (!table.isBool(stmt->condition->resolvedType)) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S077);
    dia.labels = {
        {stmt->condition->span,
         "condition has type '" + stmt->condition->resolvedType->name + "'",
         true},
    };
    dia.notes = {
        "a while condition must have type 'bool'",
    };
    dia.helps = {
        "use a boolean expression as the condition",
    };
    engine.emit(dia);
    recover.recover();
  }

  stmt->body->accept(this);
}

void Resolver::visit(SwitchStmt *stmt) {
  ScopeGuard _(table, stmt->blockScope);

  stmt->value->accept(this);

  auto *before = currentSwitch;
  currentSwitch = stmt;

  for (unsigned i = 0; i < stmt->clauses.size(); ++i) {
    auto &clause = stmt->clauses[i];
    clause->accept(this);

    if (clause->isDefault) {
      stmt->hasDefault = true;

      if (i != stmt->clauses.size() - 1) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S079);
        dia.labels = {
            {clause->span, "default case appears before another case", true},
        };
        dia.notes = {
            "the default case matches every value not handled by an earlier "
            "case",
        };
        dia.helps = {
            "move the default case to the end of the switch",
        };
        engine.emit(dia);
        recover.recover();
      }
    }

    if (clause->isWildCard) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S080);
      dia.labels = {
          {clause->span, "wildcard case is not allowed in a switch", true},
      };
      dia.notes = {
          "switch statements use 'default' for unmatched values",
          "the wildcard case '_' is only available in match expressions",
      };
      dia.helps = {
          "replace this wildcard case with a default case",
      };
      engine.emit(dia);
      recover.recover();
    }
  }

  auto *targetType = stmt->value->resolvedType;

  if (targetType->kind == TypeSymbol::TypeKind::ENUM) {
    if (stmt->usedVariants.size() != targetType->variants.size() &&
        !stmt->hasDefault) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S081);
      dia.labels = {
          {stmt->span, "this switch does not handle every enum variant", true},
      };
      dia.notes = {
          "switch statements must handle every possible enum variant",
      };
      dia.helps = {
          "add the missing enum cases or add a final default case",
      };
      engine.emit(dia);
      recover.recover();
    }
  } else if (targetType->kind == TypeSymbol::TypeKind::PRIMITIVE) {
    if (!stmt->hasDefault) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S081);
      dia.labels = {
          {stmt->span, "this switch has no default case", true},
      };
      dia.notes = {
          "primitive values cannot be exhaustively enumerated by case values",
      };
      dia.helps = {
          "add a final default case",
      };
      engine.emit(dia);
      recover.recover();
    }
  } else {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S082);
    dia.labels = {
        {stmt->value->span, "switch target has type '" + targetType->name + "'",
         true},
    };
    dia.notes = {
        "switch statements only support primitive and enum target values",
    };
    dia.helps = {
        "use a primitive or enum expression as the switch target",
    };
    engine.emit(dia);
    recover.recover();
  }

  currentSwitch = before;
}

void Resolver::visit(Case *stmt) {
  table.enter(stmt->body->blockScope);

  auto *prev = currentCase;
  currentCase = stmt;

  for (auto &value : stmt->values) {
    auto caseValue = dynamic_pointer_cast<CaseValueExpr>(value);

    if (!caseValue) {
      Error::internal(stmt->span, "illegal case value expression kind");
    }

    caseValue->accept(this);

    if (caseValue->payload && stmt->values.size() > 1) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S083);
      dia.labels = {
          {caseValue->span, "payload variant is used in a multi-value case",
           true},
      };
      dia.notes = {
          "a payload binding belongs to exactly one enum variant",
          "multi-value cases cannot introduce a payload variable",
      };
      dia.helps = {
          "place this payload variant in a separate case",
      };
      engine.emit(dia);
      recover.recover();
    }

    if (caseValue->isWildCard) {
      stmt->isWildCard = true;
    }
  }

  table.exit();
  stmt->body->accept(this);

  if (dynamic_cast<MatchExpr *>(currentSwitch)) {
    if (stmt->transfers.empty()) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S084);
      dia.labels = {
          {stmt->span, "this match case does not transfer a value", true},
      };
      dia.notes = {
          "every match case must transfer at least one result value",
      };
      dia.helps = {
          "add a value transfer statement using '<<'",
      };
      engine.emit(dia);
      recover.recover();
    }

    TypeSymbol *transferType = nullptr;

    for (auto &transfer : stmt->transfers) {
      if (!transferType) {
        transferType = transfer->resolvedType;
        continue;
      }

      if (!isAssignable(transferType, transfer->resolvedType)) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S085);
        dia.labels = {
            {transfer->span,
             "this transfer has type '" + transfer->resolvedType->name + "'",
             true},
        };
        dia.notes = {
            "previous transfers in this case have type '" + transferType->name +
                "'",
            "all value transfers in one case must have compatible types",
        };
        dia.helps = {
            "transfer a value compatible with '" + transferType->name + "'",
        };
        engine.emit(dia);
        recover.recover();
      }
    }

    stmt->transferType = transferType;

    if (stmt->values.size() != 1) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S086);
      dia.labels = {
          {stmt->span, "this match case has multiple case values", true},
      };
      dia.notes = {
          "a match case accepts exactly one selector value",
      };
      dia.helps = {
          "split these values into separate match cases",
      };
      engine.emit(dia);
      recover.recover();
    }
  }

  currentCase = prev;
}

void Resolver::visit(ReturnStmt *stmt) {
  for (Scope *scope = table.getCurrent(); scope != nullptr;
       scope = scope->parent) {
    if (scope->scopeKind == Scope::ScopeKind::INIT) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S087);
      dia.labels = {
          {stmt->span, "return statement appears inside an init method", true},
      };
      dia.notes = {
          "init methods initialize an object and cannot return explicitly",
      };
      dia.helps = {
          "remove this return statement",
      };
      engine.emit(dia);
      recover.recover();
    }
  }

  if (stmt->value != nullptr) {
    stmt->value->accept(this);

    if (stmt->value->resolvedType == nullptr) {
      Error::internal("return value type is nullptr");
    }

    stmt->returnType = static_cast<TypeSymbol *>(stmt->value->resolvedType);
  } else {
    stmt->returnType = table.getType("void");
  }

  currentMethod->returns.push_back(stmt);
}

void Resolver::visit(ValueTransferStmt *stmt) {
  if (!currentCase) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S088);
    dia.labels = {
        {stmt->span, "value transfer statement appears outside a case block",
         true},
    };
    dia.notes = {
        "value transfer statements are only available inside match cases",
    };
    dia.helps = {
        "move this statement into a match case",
    };
    engine.emit(dia);
    recover.recover();
  }

  if (dynamic_cast<SwitchStmt *>(currentSwitch)) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S088);
    dia.labels = {
        {stmt->span, "value transfer statement appears inside a switch case",
         true},
    };
    dia.notes = {
        "switch statements do not produce values",
        "value transfer statements are only available inside match cases",
    };
    dia.helps = {
        "remove this value transfer statement or use a match expression",
    };
    engine.emit(dia);
    recover.recover();
  }

  if (!dynamic_cast<MatchExpr *>(currentSwitch)) {
    Error::internal("value transfer reached an invalid switch context");
  }

  stmt->value->accept(this);
  currentCase->transfers.push_back(stmt->value);
}

void Resolver::visit(BreakStmt *) {}

void Resolver::visit(ContinueStmt *) {}

void Resolver::visit(DeclStmt *stmt) { stmt->decl->accept(this); }

void Resolver::visit(EmptyStmt *) {}