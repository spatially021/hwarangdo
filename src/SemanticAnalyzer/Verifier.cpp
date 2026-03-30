#include "SemanticAnalyzer/Verifier.h"
#include "AST/Decl.h"
#include "AST/Expr.h"
#include "AST/Stmt.h"
#include "util/Error.h"
#include <cerrno>

void Verifier::visit(LiteralExpr *expr) {
  if (expr->resolvedType == nullptr)
    unresolved(expr, "literalExpr is unresovled");
}
void Verifier::visit(BinaryExpr *expr) {

  expr->left->accept(this);
  expr->right->accept(this);
  if (expr->resolvedType == nullptr)
    unresolved(expr, "binaryExpr is unresolved");
}
void Verifier::visit(NameExpr *expr) {
  if (expr->resolvedType == nullptr) {
    unresolved(expr, "nameExpr is unresolved");
  }

  if (expr->resolvedType == nullptr)
    unresolved(expr, "varExpr's type is unresolved");
}
void Verifier::visit(UnaryExpr *expr) {
  expr->right->accept(this);
  if (expr->resolvedType == nullptr)
    unresolved(expr, "unaryExpr is unresolved");
}
void Verifier::visit(CallExpr *expr) {
  for (auto &a : expr->arguments) {
    a->accept(this);
  }
  if (expr->receiver != nullptr)
    expr->receiver->accept(this);
  if (expr->callType == CallExpr::CallType::FUNC_CALL) {
    if (expr->resolved == nullptr)
      unresolved(expr, "method is unresolved");
  } else if (expr->callType == CallExpr::CallType::PAYLOAD_CALL) {
    if (expr->resolved == nullptr)
      unresolved(expr, "variant is unresolved");
  } else {
    unresolved(expr, "callExpr unresolved");
  }
}
void Verifier::visit(AssignExpr *expr) {
  expr->target->accept(this);
  expr->value->accept(this);
  if (expr->resolvedType == nullptr)
    unresolved(expr, "assignExpr is unresolved");
}
void Verifier::visit(MemberExpr *expr) {
  expr->object->accept(this);
  if (expr->resolved == nullptr)
    unresolved(expr, "memberExpr is unresolved");
}
void Verifier::visit(ArrayAccessExpr *expr) {
  expr->object->accept(this);
  expr->index->accept(this);
  if (expr->resolvedType == nullptr)
    unresolved(expr, "arrayAccessExpr is unresolved");
}
void Verifier::visit(TernaryExpr *expr) {
  expr->conditon->accept(this);
  expr->else_->accept(this);
  expr->then->accept(this);
  if (expr->resolvedType == nullptr)
    unresolved(expr, "ternarExpr is unresolved");
}
void Verifier::visit(ThisExpr *) {}
void Verifier::visit(SuperExpr *) {}
void Verifier::visit(RootExpr *) {}
void Verifier::visit(SelfExpr *) {}
void Verifier::visit(CastExpr *expr) {
  expr->left->accept(this);
  expr->type->accept(this);
  if (expr->resolvedType == nullptr) {
    unresolved(expr, "castExpr is unresolved");
  }
}
void Verifier::visit(SpawnExpr *expr) {
  expr->left->accept(this);
  expr->spawnType->accept(this);
  for (auto &p : expr->args) {
    p->accept(this);
  }
  if (expr->resolvedType == nullptr) {
    unresolved(expr, "castExpr is unresolved");
  }
}
void Verifier::visit(ViewExpr *expr) {
  expr->left->accept(this);
  expr->target->accept(this);
  if (expr->resolvedType == nullptr) {
    unresolved(expr, "castExpr is unresolved");
  }
}
void Verifier::visit(DefaultValueExpr *) {}
void Verifier::visit(Range *expr) {
  expr->from->accept(this);
  expr->to->accept(this);
  if (expr->step) {
    expr->step->accept(this);
  }
}
void Verifier::visit(CaseValueExpr *expr) {
  expr->value->accept(this);
  if (expr->arg) {
    expr->arg->accept(this);
  }
}
void Verifier::visit(MatchExpr *expr) {
  expr->value->accept(this);
  for (auto &c : expr->cases) {
    c->accept(this);
  }
  if (!expr->resolvedType) {
    unresolved(expr, "unresolved match type");
  }
}
// Statement Verifier::visitor methods
void Verifier::visit(ExprStmt *stmt) { stmt->expr->accept(this); }
void Verifier::visit(BlockStmt *stmt) {
  for (auto &s : stmt->statements) {
    s->accept(this);
  }
}
void Verifier::visit(BuiltInNameExpr *) {}

void Verifier::visit(IfStmt *stmt) {
  stmt->condition->accept(this);
  stmt->thenBranch->accept(this);
  stmt->elseBranch->accept(this);
}
void Verifier::visit(ForStmt *stmt) {
  stmt->initializer->accept(this);
  stmt->range->accept(this);
  stmt->body->accept(this);
}
void Verifier::visit(WhileStmt *stmt) {
  stmt->condition->accept(this);
  stmt->body->accept(this);
}
void Verifier::visit(SwitchStmt *stmt) {
  stmt->value->accept(this);
  for (auto c : stmt->clauses) {
    c->accept(this);
  }
}
void Verifier::visit(Case *stmt) { stmt->body->accept(this); }
void Verifier::visit(ReturnStmt *stmt) {
  if (stmt->value != nullptr) {
    if (stmt->returnType == nullptr)
      unresolved(stmt, "returnStmt is unresolved");
  }
}
void Verifier::visit(ValueTransferStmt *stmt) { stmt->value->accept(this); }
void Verifier::visit(BreakStmt *) {}
void Verifier::visit(ContinueStmt *) {}
void Verifier::visit(DeclStmt *stmt) { stmt->decl->accept(this); }
void Verifier::visit(EmptyStmt *) {}

// declare Verifier::visitor methods
void Verifier::visit(ClassDecl *decl) {
  if (decl->symbol == nullptr)
    unresolved(decl, "ClassDecl is unresolved");
  for (auto &a : decl->fields) {
    a->accept(this);
  }
  for (auto &a : decl->methods) {
    a->accept(this);
  }
  for (auto &a : decl->innerDecl) {
    a->accept(this);
  }
}
void Verifier::visit(StructDecl *decl) {
  if (decl->symbol == nullptr)
    unresolved(decl, "structDecl is unresolved");
  for (auto &f : decl->fields) {
    f->accept(this);
  }
}
void Verifier::visit(EnumDecl *decl) {
  if (decl->symbol == nullptr)
    unresolved(decl, "EnumDecl is unresolved");
}
void Verifier::visit(ImplDecl *) {}
void Verifier::visit(TraitDecl *) {}
void Verifier::visit(TraitSig *sig) {
  if (sig->symbol == nullptr)
    unresolved(sig, "trait signiture is unresolved");
  for (auto &p : sig->params) {
    p->accept(this);
  }
}
void Verifier::visit(FuncDecl *decl) {
  if (decl->methodSymbol == nullptr)
    unresolved(decl, "funcDecl is unresolved");
  for (auto &p : decl->params)
    p->accept(this);
  decl->body->accept(this);
}
void Verifier::visit(VarDecl *decl) {
  if (decl->symbol == nullptr)
    unresolved(decl, "varDecl is unresolved");
}

void Verifier::visit(TypeNode *) {}
void Verifier::visit(ASTNode *) {}
void Verifier::visit(Param *param) {
  if (param->symbol == nullptr)
    unresolved(param, "param is unresolved");
}

void Verifier::visit(InitDecl *decl) {
  if (decl->methodSymbol == nullptr) {
    unresolved(decl, "init is unresolved");
  }
  for (auto &p : decl->params)
    p->accept(this);
  decl->body->accept(this);
}

void Verifier::unresolved(ASTNode *node, const string &msg) {
  Error::internal(node->token, msg);
}
