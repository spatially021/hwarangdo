#include "AST/Decl.h"
#include "AST/Expr.h"
#include "AST/Stmt.h"
#include "SemanticAnalyzer/Guard.h"
#include "SemanticAnalyzer/Resolver.h"
#include "SemanticAnalyzer/Scope.h"
#include "SemanticAnalyzer/symbol/MethodSymbol.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "SemanticAnalyzer/symbol/ValueSymbol.h"
#include "util/Error.h"
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
    Error::diagnostic(stmt->condition->token, "condition is not bool type");
  }

  stmt->thenBranch->accept(this);
  if (stmt->elseBranch != nullptr)
    stmt->elseBranch->accept(this);
}
void Resolver::visit(ForStmt *stmt) {
  stmt->initializer->accept(this);
  stmt->range->accept(this);
  ScopeGuard _(*table, stmt->blockScope);
  stmt->body->accept(this);
}
void Resolver::visit(WhileStmt *stmt) {
  stmt->condition->accept(this);
  if (!table->isBool(stmt->condition->resolvedType))
    Error::diagnostic(stmt->condition->token, "condition is not bool type");
  stmt->body->accept(this);
}
void Resolver::visit(SwitchStmt *stmt) {
  ScopeGuard _(*table, stmt->blockScope);
  stmt->value->accept(this);
  auto before = currentSwitch;
  currentSwitch = stmt;
  for (auto c : stmt->clauses)
    c->accept(this);
  currentSwitch = before;
}
void Resolver::visit(Case *stmt) {
  table->enter(stmt->body->blockScope);
  auto prev = currentCase;
  currentCase = stmt;
  for (auto v : stmt->values) {
    auto t = dynamic_pointer_cast<CaseValueExpr>(v);
    if (!t) {
      Error::internal(stmt->token, "illegal expr kind");
    }
    t->accept(this);
    if (t->payloadType) {
      auto symbol = make_unique<ValueSymbol>();
      symbol->name = t->arg->token.text;
      symbol->kind = ValueSymbol::Kind::VAR;
      symbol->typeSymbol = t->payloadType;
      symbol->node = stmt;
      table->add(std::move(symbol));
    }
  }
  table->exit();
  stmt->body->accept(this);
  if (dynamic_cast<MatchExpr *>(currentSwitch)) {
    if (stmt->transfers.empty()) {
      Error::diagnostic(stmt->token,
                        "at least one value transfer need in match's case");
    }
    TypeSymbol *type = nullptr;

    for (auto t : stmt->transfers) {
      if (!type) {
        type = t->resolvedType;
        continue;
      }
      if (!isAssignable(t->resolvedType, type)) {
        Error::diagnostic(t->token, "inconsistent value transfer");
      }
    }

    stmt->transferType = type;
  }

  currentCase = prev;
}

void Resolver::visit(ReturnStmt *stmt) {
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
    Error::diagnostic(stmt->token, "<< allow in case statement");
  }
  if (dynamic_cast<SwitchStmt *>(currentSwitch)) {
    Error::diagnostic(stmt->token, "<< not allow in swtich statement");
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
