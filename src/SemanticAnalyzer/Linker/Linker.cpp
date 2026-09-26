#include "hrd/SemanticAnalyzer/Linker.h"
#include "hrd/AST/Decl.h"
#include "hrd/AST/Expr.h"
#include "hrd/AST/Stmt.h"
#include "hrd/SemanticAnalyzer/SymbolTable/SymbolTable.h"
#include "hrd/compiler/CompilerContexts.h"
#include "hrd/util/Guard.h"
#include "hrd/util/Helper.h"
#include <cassert>

Linker::Linker(LinkerContext &ctx)
    : table(ctx.table), currentType(ctx.table.registry.getCurrent()),
      engine(ctx.engine), recover(*this) {}

void Linker::visit(LiteralExpr *) {}
void Linker::visit(BinaryExpr *expr) {
  expr->left->accept(this);
  expr->right->accept(this);
}
void Linker::visit(NameExpr *) {}
void Linker::visit(UnaryExpr *expr) { expr->right->accept(this); }
void Linker::visit(CallExpr *expr) {
  if (expr->receiver != nullptr) {
    expr->receiver->accept(this);
  }
  for (auto &a : expr->arguments) {
    a->accept(this);
  }
}
void Linker::visit(AssignExpr *expr) {
  expr->target->accept(this);
  expr->value->accept(this);
}
void Linker::visit(MemberExpr *expr) { expr->object->accept(this); }
void Linker::visit(ArrayAccessExpr *expr) {
  expr->object->accept(this);
  expr->index->accept(this);
}
void Linker::visit(TernaryExpr *expr) {
  expr->conditon->accept(this);
  expr->then->accept(this);
  expr->else_->accept(this);
}
void Linker::visit(ThisExpr *) {}
void Linker::visit(SuperExpr *) {}
void Linker::visit(RootExpr *) {}
void Linker::visit(SelfExpr *) {}
void Linker::visit(CastExpr *) {}
void Linker::visit(BuiltInNameExpr *) {}
void Linker::visit(SpawnExpr *expr) {
  expr->left->accept(this);
  expr->spawnType->accept(this);
}
void Linker::visit(ViewExpr *expr) {
  expr->left->accept(this);
  expr->target->accept(this);
}
void Linker::visit(DestroyExpr *expr) {
  expr->storage->accept(this);
  expr->target->accept(this);
}
void Linker::visit(QuitExpr *) {}
void Linker::visit(DefaultValueExpr *) {}
void Linker::visit(Range *expr) {
  expr->from->accept(this);
  expr->to->accept(this);
  if (expr->step) {
    expr->step->accept(this);
  }
}
void Linker::visit(CaseValueExpr *expr) {
  expr->value->accept(this);
  if (expr->arg) {
    expr->arg->accept(this);
  }
}
void Linker::visit(MatchExpr *expr) {
  ScopeGuard _(table, expr->blockScope);
  expr->value->accept(this);
  for (auto &c : expr->clauses) {
    c->accept(this);
  }
}
void Linker::visit(ArrayLiteralExpr *expr) {
  for (auto &e : expr->elements) {
    e->accept(this);
  }
}

// Statement Linker::visitor methods
void Linker::visit(ExprStmt *stmt) { stmt->expr->accept(this); }
void Linker::visit(BlockStmt *stmt) {
  ScopeGuard _(table, stmt->blockScope);
  for (auto s : stmt->statements) {
    s->accept(this);
  }
}
void Linker::visit(IfStmt *stmt) {
  stmt->thenBranch->accept(this);
  if (stmt->elseBranch != nullptr) {
    stmt->elseBranch->accept(this);
  }
}
void Linker::visit(ForStmt *stmt) {
  ScopeGuard _(table, stmt->blockScope);
  stmt->initializer->accept(this);
  stmt->range->accept(this);
  stmt->body->accept(this);
}
void Linker::visit(WhileStmt *stmt) { stmt->body->accept(this); }
void Linker::visit(SwitchStmt *stmt) {
  ScopeGuard _(table, stmt->blockScope);
  for (auto &c : stmt->clauses) {
    c->accept(this);
  }
}
void Linker::visit(Case *c) { c->body->accept(this); }
void Linker::visit(ReturnStmt *stmt) {
  if (stmt->value != nullptr) {
    stmt->value->accept(this);
  }
}
void Linker::visit(ValueTransferStmt *stmt) { stmt->value->accept(this); }
void Linker::visit(BreakStmt *) {}
void Linker::visit(ContinueStmt *) {}

void Linker::visit(EmptyStmt *) {}
void Linker::visit(ImportDecl *) {}
void Linker::visit(ASTNode *) {}

void Linker::visit(VarDecl *decl) {
  decl->type->accept(this);
  decl->symbol->typeSymbol = decl->type->resolved;
  if (decl->init) {
    decl->init->accept(this);
  }
}