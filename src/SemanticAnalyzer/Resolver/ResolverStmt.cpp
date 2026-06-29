#include "hrd/AST/Decl.h"
#include "hrd/AST/Expr.h"
#include "hrd/AST/Stmt.h"
#include "hrd/SemanticAnalyzer/Resolver.h"
#include "hrd/SemanticAnalyzer/Scope.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/util/Error.h"
#include "hrd/util/Guard.h"
#include <memory>

// Statement Resolver::visitor methods
void Resolver::visit(ExprStmt *stmt) { stmt->expr->accept(this); }
void Resolver::visit(BlockStmt *stmt) {
  ScopeGuard _(*table, stmt->blockScope);
  for (auto a : stmt->statements)
    a->accept(this);
}
void Resolver::visit(IfStmt *stmt) {
  stmt->condition->accept(this);
  if (!table->isBool(stmt->condition->resolvedType)) {
    Error::diagnostic(stmt->condition->span, "condition is not bool type");
  }

  stmt->thenBranch->accept(this);
  if (stmt->elseBranch != nullptr)
    stmt->elseBranch->accept(this);
}
void Resolver::visit(ForStmt *stmt) {
  stmt->initializer->accept(this);
  stmt->range->accept(this);
  if (auto var = dynamic_cast<VarDecl *>(stmt->initializer->decl.get())) {
    if (!isAssignable(var->symbol->typeSymbol, stmt->range->resolvedType)) {
      Error::diagnostic(stmt->span, "cannot cast initalizer to rangeType");
    }
  } else {
    Error::internal(stmt->initializer->span, "initializer is not varDecl");
  }
  ScopeGuard _(*table, stmt->blockScope);
  stmt->body->accept(this);
}
void Resolver::visit(WhileStmt *stmt) {
  stmt->condition->accept(this);
  if (!table->isBool(stmt->condition->resolvedType))
    Error::diagnostic(stmt->condition->span, "condition is not bool type");
  stmt->body->accept(this);
}
void Resolver::visit(SwitchStmt *stmt) {
  ScopeGuard _(*table, stmt->blockScope);
  stmt->value->accept(this);
  auto before = currentSwitch;
  currentSwitch = stmt;

  for (unsigned i = 0; i < stmt->clauses.size(); ++i) {
    stmt->clauses[i]->accept(this);
    if (stmt->clauses[i]->isDefault) {
      stmt->hasDefault = true;
      if (i != stmt->clauses.size() - 1) {
        Error::diagnostic(stmt->clauses[i]->span,
                          "default only place last of switch");
      }
    }
    if (stmt->clauses[i]->isWildCard) {
      Error::diagnostic(stmt->clauses[i]->span, "_in switch not allow '_'");
    }
  }

  if (stmt->value->resolvedType->kind == TypeSymbol::TypeKind::PRIMITIVE) {
    // TODO: 경고 추가시 경고 로직 넣기.
  }

  if (stmt->usedVariants.size() != stmt->value->resolvedType->variants.size()) {
    if (!stmt->hasDefault) {
      Error::diagnostic(stmt->span,
                        "has missing variant but no defualt in switch");
    }
  }

  currentSwitch = before;
}
void Resolver::visit(Case *stmt) {
  table->enter(stmt->body->blockScope);
  auto prev = currentCase;
  currentCase = stmt;
  for (auto v : stmt->values) {
    auto t = dynamic_pointer_cast<CaseValueExpr>(v);
    if (!t) {
      Error::internal(stmt->span, "illegal expr kind");
    }
    t->accept(this);
    if (t->payload && stmt->values.size() > 1) {
      Error::diagnostic(stmt->span,
                        "payload case cannot be used in multi-value case");
    }
    if (t->isWildCard) {
      stmt->isWildCard = true;
    }
  }
  table->exit();
  stmt->body->accept(this);
  if (dynamic_cast<MatchExpr *>(currentSwitch)) {
    if (stmt->transfers.empty()) {
      Error::diagnostic(stmt->span,
                        "at least one value transfer need in match's case");
    }
    TypeSymbol *type = nullptr;

    for (auto t : stmt->transfers) {
      if (!type) {
        type = t->resolvedType;
        continue;
      }
      if (!isAssignable(t->resolvedType, type)) {
        Error::diagnostic(t->span, "inconsistent value transfer");
      }
    }

    stmt->transferType = type;

    if (stmt->values.size() != 1) {
      Error::diagnostic(stmt->span, "in match only one case key allowed");
    }
  }

  currentCase = prev;
}

void Resolver::visit(ReturnStmt *stmt) {

  for (Scope *s = table->getCurrent(); s != nullptr; s = s->parent) {
    if (s->scopeKind == Scope::ScopeKind::INIT) {
      Error::diagnostic(stmt->span, "in init method cannot use return");
    }
  }

  if (stmt->value != nullptr) {
    stmt->value->accept(this);
    if (stmt->value->resolvedType == nullptr) {
      Error::internal("return value is nullptr");
    }
    stmt->returnType = static_cast<TypeSymbol *>(stmt->value->resolvedType);
  } else {
    stmt->returnType = table->getType("void");
  }
  currentMethod->returns.push_back(stmt);
}

void Resolver::visit(ValueTransferStmt *stmt) {
  if (!currentCase) {
    Error::diagnostic(stmt->span, "<< allow in case statement");
  }
  if (dynamic_cast<SwitchStmt *>(currentSwitch)) {
    Error::diagnostic(stmt->span, "<< not allow in swtich statement");
  } else if (!dynamic_cast<MatchExpr *>(currentSwitch)) {
    Error::internal("illegal node kind");
  }
  stmt->value->accept(this);
  currentCase->transfers.push_back(stmt->value);
}

void Resolver::visit(BreakStmt *) {}
void Resolver::visit(ContinueStmt *) {}
void Resolver::visit(DeclStmt *stmt) { stmt->decl->accept(this); }
void Resolver::visit(EmptyStmt *) {}
