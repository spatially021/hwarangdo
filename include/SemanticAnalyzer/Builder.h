#pragma once

#include "AST/Decl.h"
#include "AST/Stmt.h"
#include "AST/Visitor.h"
#include "SymbolTable.h"
#include "util/Error.h"
#include <cassert>
#include <memory>

class Builder : public ASTVisitor {
public:
  SymbolTable *table = nullptr;
  TypeSymbol *currentType = nullptr;
  unique_ptr<Scope> rootScope = make_unique<Scope>();
  Builder(SymbolTable *table);

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
  void visit(BreakStmt *stmt);
  void visit(ContinueStmt *stmt);
  void visit(DeclStmt *stmt);
  void visit(EmptyStmt *stmt);
  void visit(ValueTransferStmt *stmt);
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

  void visit(TypeNode *decl);
  void visit(ASTNode *node);
  void visit(Param *param);
  inline void linkRoot() {
    if (!table->main) {
      Error::diagnostic({}, "has no main");
    }
    table->main->rootScope = std::move(rootScope);
  }

  void visit(InitDecl *decl);

private:
  unique_ptr<TypeSymbol> topLevel;
  void extracted();
  void buildMain(ClassDecl *decl);
  bool canInnerDecl(Decl *decl);
};
