#include "SemanticAnalyzer/Verifier.h"
#include "AST/Decl.h"
#include "AST/Expr.h"
#include "util/Error.h"
#include <cerrno>
#include <string>

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
void Verifier::visit(VarExpr *expr) {
  if (expr->resolved == nullptr)
    unresolved(expr, "varExpr is unresolved");
  if (expr->resolvedType == nullptr)
    unresolved(expr, "varExpr's type is unresolved");
}
void Verifier::visit(UnaryExpr *expr) {
  expr->right->accept(this);
  if (expr->resolvedType == nullptr)
    unresolved(expr, "unaryExpr is unresolved");
}
void Verifier::visit(CallExpr *expr) {
  for (auto a : expr->arguments) {
    a->accept(this);
  }
  if (expr->receiver != nullptr)
    expr->receiver->accept(this);
  if (expr->callType == CallExpr::CallType::FUNC_CALL) {
    if (expr->methodResolved == nullptr)
      unresolved(expr, "method is unresolved");
  } else if (expr->callType == CallExpr::CallType::PAYLOAD_CALL) {
    if (expr->VariantResolved == nullptr)
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
  if (expr->resolvedType == nullptr)
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

// Statement Verifier::visitor methods
void Verifier::visit(ExprStmt *stmt) { stmt->expr->accept(this); }
void Verifier::visit(BlockStmt *stmt) {
  for (auto s : stmt->statements) {
    s->accept(this);
  }
}
void Verifier::visit(IfStmt *stmt) {
  stmt->accept(this);
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
    if (stmt->resolved == nullptr)
      unresolved(stmt, "returnStmt is unresolved");
  }
}
void Verifier::visit(BreakStmt *) {}
void Verifier::visit(ContinueStmt *) {}
void Verifier::visit(DeclStmt *stmt) { stmt->decl->accept(this); }
void Verifier::visit(EmptyStmt *) {}

// declare Verifier::visitor methods
void Verifier::visit(ClassDecl *decl) {
  if (decl->symbol == nullptr)
    unresolved(decl, "ClassDecl is unresolved");
  for (auto a : decl->body) {
    a->accept(this);
  }
}
void Verifier::visit(StructDecl *decl) {
  if (decl->symbol == nullptr)
    unresolved(decl, "structDecl is unresolved");
  for (auto f : decl->fields) {
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
  for (auto p : sig->params) {
    p->accept(this);
  }
}
void Verifier::visit(FuncDecl *decl) {
  if (decl->methodSymbol == nullptr)
    unresolved(decl, "funcDecl is unresolved");
  for (auto p : decl->params)
    p->accept(this);
  decl->body->accept(this);
}
void Verifier::visit(VarDecl *decl) {
  if (decl->symbol == nullptr)
    unresolved(decl, "varDecl is unresolved");
}
void Verifier::visit(ArrayDecl *decl) {
  if (decl->symbol == nullptr)
    unresolved(decl, "ArrayDecl is unresolved");
}

void Verifier::visit(TypeNode *) {}
void Verifier::visit(ASTNode *) {}
void Verifier::visit(Param *param) {
  if (param->symbol == nullptr)
    unresolved(param, "param is unresolved");
}

void Verifier::unresolved(ASTNode *node, const string &msg) {
  Error::internal(std::to_string(node->token.line) + ":" +
                  std::to_string(node->token.col) + " " + msg);
}