#include "AST/Decl.h"
#include "AST/Expr.h"
#include "AST/Stmt.h"
#include "IR/HIR/HIRBuilder.h"
#include "IR/HIR/HIRExpr.h"
#include "IR/HIR/HIRStmt.h"
#include "IR/HIR/HIRType.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "util/Error.h"
#include <memory>
#include <utility>

using std::unique_ptr;

void HIRBuilder::visit(LiteralExpr *expr) {
  auto ty = lowerType(expr->resolvedLit.type);
  exprResult = make_unique<HIRLiteralExpr>(ty, expr->resolvedLit);
}
void HIRBuilder::visit(BinaryExpr *expr) {}
void HIRBuilder::visit(NameExpr *expr) {
  if (expr->valueSymbol) {
    if (expr->valueSymbol->typeSymbol->kind == TypeSymbol::TypeKind::CLASS) {
      auto type = lowerEntityType(expr->valueSymbol->typeSymbol);
    }
  } else {
  }
}
void HIRBuilder::visit(UnaryExpr *expr) {}
void HIRBuilder::visit(CallExpr *expr) {}
void HIRBuilder::visit(AssignExpr *expr) {}
void HIRBuilder::visit(MemberExpr *expr) {}
void HIRBuilder::visit(ArrayAccessExpr *expr) {}
void HIRBuilder::visit(TernaryExpr *expr) {}
void HIRBuilder::visit(ThisExpr *expr) {}
void HIRBuilder::visit(SuperExpr *expr) {}
void HIRBuilder::visit(CastExpr *expr) {}
void HIRBuilder::visit(BuiltInNameExpr *expr) {}
void HIRBuilder::visit(SpawnExpr *expr) {}
void HIRBuilder::visit(ViewExpr *expr) {}
void HIRBuilder::visit(DefaultValueExpr *expr) {}
void HIRBuilder::visit(Range *expr) {}
void HIRBuilder::visit(CaseValueExpr *expr) {}
void HIRBuilder::visit(MatchExpr *expr) {}

// Statement HIRBuilder::visitor methods
void HIRBuilder::visit(ExprStmt *stmt) {
  auto expr = lowerExpr(stmt->expr.get());
  emit(make_unique<HIRExprStmt>(std::move(expr)));
}
void HIRBuilder::visit(BlockStmt *stmt) { emit(lowerBlock(stmt)); }
void HIRBuilder::visit(IfStmt *stmt) {
  unique_ptr<HIRExpr> cond = lowerExpr(stmt->condition.get());
  unique_ptr<HIRBlockStmt> thenBlock = lowerStmtAsBlock(stmt->thenBranch.get());
  unique_ptr<HIRBlockStmt> elseBlock =
      stmt->elseBranch ? lowerStmtAsBlock(stmt->elseBranch.get()) : nullptr;
  emit(make_unique<HIRIfStmt>(std::move(cond), std::move(thenBlock),
                              std::move(elseBlock)));
}
void HIRBuilder::visit(ForStmt *) {}
void HIRBuilder::visit(WhileStmt *stmt) {
  unique_ptr<HIRExpr> cond = lowerExpr(stmt->condition.get());
  unique_ptr<HIRBlockStmt> body = lowerStmtAsBlock(stmt->body.get());
  emit(make_unique<HIRWhileStmt>(std::move(cond), std::move(body)));
}
void HIRBuilder::visit(SwitchStmt *stmt) {}
void HIRBuilder::visit(Case *stmt) {}

void HIRBuilder::visit(ReturnStmt *stmt) {
  std::unique_ptr<HIRExpr> expr = nullptr;
  if (stmt->value) {
    expr = lowerExpr(stmt->value.get());
  }
  emit(make_unique<HIRReturnStmt>(std::move(expr)));
}

void HIRBuilder::visit(ValueTransferStmt *stmt) {}
void HIRBuilder::visit(BreakStmt *) { emit(make_unique<HIRBreakStmt>()); }
void HIRBuilder::visit(ContinueStmt *) { emit(make_unique<HIRContinueStmt>()); }
void HIRBuilder::visit(DeclStmt *stmt) {}
void HIRBuilder::visit(EmptyStmt *) {}

// declare HIRBuilder::visitor methods
void HIRBuilder::visit(ClassDecl *decl) {}
void HIRBuilder::visit(StructDecl *decl) {}
void HIRBuilder::visit(EnumDecl *decl) {}
void HIRBuilder::visit(ImplDecl *decl) {}
void HIRBuilder::visit(TraitDecl *decl) {}
void HIRBuilder::visit(TraitSig *decl) {}
void HIRBuilder::visit(FuncDecl *decl) {}
void HIRBuilder::visit(VarDecl *decl) {}
void HIRBuilder::visit(ArrayDecl *decl) {}
void HIRBuilder::visit(InitDecl *decl) {}

void HIRBuilder::visit(TypeNode *) {}
void HIRBuilder::visit(ASTNode *node) {
  Error::internal(node->token, "unknown generic");
}
void HIRBuilder::visit(Param *) {}