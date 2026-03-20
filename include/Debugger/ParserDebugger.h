#pragma once

#include "AST/Expr.h"
#include "AST/Visitor.h"
#include <cstddef>
#include <iostream>
#include <string>
class ParserDebugger : public ASTVisitor {
public:
  std::size_t depth = 0;
  std::string ident();
  void visit(LiteralExpr *expr);
  void visit(BinaryExpr *expr);
  void visit(NameExpr *expr);
  void visit(UnaryExpr *expr);
  void visit(CallExpr *expr);
  void visit(AssignExpr *expr);
  void visit(MemberExpr *expr);
  void visit(ArrayAccessExpr *expr);
  void visit(TernaryExpr *expr);
  void visit(ThisExpr *expr);
  void visit(SuperExpr *expr);
  void visit(CastExpr *expr);
  void visit(BuiltInNameExpr *expr);
  void visit(SpawnExpr *expr);
  void visit(ViewExpr *expr);
  void visit(DefaultValueExpr *expr);
  void visit(Range *expr);
  void visit(CaseValueExpr *expr);
  void visit(MatchExpr *expr);

  // Statement visitor methods
  void visit(ExprStmt *stmt);
  void visit(BlockStmt *stmt);
  void visit(IfStmt *stmt);
  void visit(ForStmt *stmt);
  void visit(WhileStmt *stmt);
  void visit(SwitchStmt *stmt);
  void visit(Case *stmt);
  void visit(ReturnStmt *stmt);
  void visit(ValueTransferStmt *stmt);
  void visit(BreakStmt *stmt);
  void visit(ContinueStmt *stmt);
  void visit(DeclStmt *stmt);
  void visit(EmptyStmt *stmt);

  // declare visitor methods
  void visit(ClassDecl *decl);
  void visit(StructDecl *decl);
  void visit(EnumDecl *decl);
  void visit(ImplDecl *decl);
  void visit(TraitDecl *decl);
  void visit(TraitSig *decl);
  void visit(FuncDecl *decl);
  void visit(VarDecl *decl);
  void visit(ArrayDecl *decl);
  void visit(InitDecl *decl);

  void visit(TypeNode *decl);
  void visit(ASTNode *node);
  void visit(Param *param);

private:
  template <typename Container, typename Func>
  void join(const Container &c, const char *sep, Func f) {
    bool first = true;
    for (const auto &elem : c) {
      if (!first)
        cout << sep;
      f(elem);
      first = false;
    }
  }

  template <typename Container>
  void joinAccept(const Container &c, const char *sep) {
    join(c, sep, [this](const auto &elem) { elem->accept(this); });
  }
};