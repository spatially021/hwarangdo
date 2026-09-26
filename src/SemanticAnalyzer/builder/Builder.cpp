#include "hrd/SemanticAnalyzer/Builder.h"
#include "hrd/AST/ASTNode.h"
#include "hrd/AST/Decl.h"
#include "hrd/AST/Expr.h"
#include "hrd/Recover/BuilderRecover.h"
#include "hrd/SemanticAnalyzer/Scope.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/compiler/CompilerContexts.h"
#include "hrd/util/Guard.h"
#include <memory>

Builder::Builder(BuilderContext &ctx)
    : table(ctx.table), currentType(ctx.table.registry.getCurrent()),
      engine(ctx.engine), recover(*this) {
  topLevel = make_unique<TypeSymbol>();
  topLevel->name = "<top-level>";
  currentType = topLevel.get();
  rootScope->id = -1;
}

void Builder::visit(LiteralExpr *) {}
void Builder::visit(BinaryExpr *expr) {
  expr->left->accept(this);
  expr->right->accept(this);
}
void Builder::visit(NameExpr *) {}
void Builder::visit(UnaryExpr *expr) { expr->right->accept(this); }
void Builder::visit(CallExpr *expr) {
  if (expr->receiver != nullptr) {
    expr->receiver->accept(this);
  }
  for (auto a : expr->arguments) {
    a->accept(this);
  }
}
void Builder::visit(AssignExpr *expr) {
  expr->value->accept(this);
  expr->target->accept(this);
}
void Builder::visit(MemberExpr *expr) { expr->object->accept(this); }
void Builder::visit(ArrayAccessExpr *expr) {
  expr->object->accept(this);
  expr->index->accept(this);
}
void Builder::visit(TernaryExpr *expr) {
  expr->conditon->accept(this);
  expr->then->accept(this);
  if (expr->else_) {
    expr->else_->accept(this);
  }
}
void Builder::visit(ThisExpr *) {}
void Builder::visit(SuperExpr *) {}
void Builder::visit(RootExpr *) {};
void Builder::visit(SelfExpr *) {};
void Builder::visit(CastExpr *expr) {
  expr->left->accept(this);
  expr->type->accept(this);
}
void Builder::visit(BuiltInNameExpr *) {}
void Builder::visit(SpawnExpr *expr) {
  expr->left->accept(this);
  for (auto &a : expr->args) {
    a->accept(this);
  }
}
void Builder::visit(ViewExpr *expr) {
  expr->left->accept(this);
  expr->target->accept(this);
}
void Builder::visit(DestroyExpr *expr) {
  expr->storage->accept(this);
  expr->target->accept(this);
}
void Builder::visit(QuitExpr *) {}
void Builder::visit(DefaultValueExpr *) {}
void Builder::visit(Range *expr) {
  expr->from->accept(this);
  expr->to->accept(this);
}
void Builder::visit(CaseValueExpr *expr) {
  expr->value->accept(this);
  if (expr->arg) {
    expr->arg->accept(this);
  }
}
void Builder::visit(MatchExpr *expr) {
  ScopeGuard _(table);
  table.scopeManger.current()->scopeKind = Scope::ScopeKind::BLOCK;
  expr->blockScope = table.scopeManger.current();
  for (auto &c : expr->clauses) {
    c->accept(this);
  }
}

void Builder::visit(ArrayLiteralExpr *expr) {
  for (auto &e : expr->elements) {
    e->accept(this);
  }
}

// Statement Builder::visitor methods
void Builder::visit(ExprStmt *stmt) { stmt->expr->accept(this); }
void Builder::visit(BlockStmt *stmt) {
  ScopeGuard _(table);
  stmt->blockScope = table.scopeManger.current();
  table.scopeManger.current()->scopeKind = Scope::ScopeKind::BLOCK;
  for (auto &s : stmt->statements) {
    s->accept(this);
  }
}
void Builder::visit(IfStmt *stmt) {
  stmt->thenBranch->accept(this);
  if (stmt->elseBranch != nullptr)
    stmt->elseBranch->accept(this);
}
void Builder::visit(ForStmt *stmt) {
  ScopeGuard _(table);
  stmt->blockScope = table.scopeManger.current();
  stmt->initializer->accept(this);
  table.scopeManger.current()->scopeKind = Scope::ScopeKind::BLOCK;
  stmt->body->accept(this);
}

void Builder::visit(WhileStmt *stmt) { stmt->body->accept(this); }

void Builder::visit(SwitchStmt *stmt) {
  ScopeGuard _(table);
  table.scopeManger.current()->scopeKind = Scope::ScopeKind::BLOCK;
  stmt->blockScope = table.scopeManger.current();

  for (auto &c : stmt->clauses) {
    c->accept(this);
  }
}
void Builder::visit(Case *stmt) { stmt->body->accept(this); }

void Builder::visit(ReturnStmt *stmt) {
  if (stmt->value) {
    stmt->value->accept(this);
  }
}
void Builder::visit(ValueTransferStmt *stmt) { stmt->value->accept(this); }
void Builder::visit(BreakStmt *) {}
void Builder::visit(ContinueStmt *) {}

void Builder::visit(DeclStmt *stmt) { stmt->decl->accept(this); }

void Builder::visit(EmptyStmt *) {}

void Builder::visit(TypeNode *) {}
void Builder::visit(ASTNode *) {}
