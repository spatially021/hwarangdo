#pragma once

#include "../AST/Visitor.h"
#include "Symbol.h"
#include "SymbolTable.h"
#include <cassert>
#include <memory>

class Builder : public ASTVisitor {
public:
  SymbolTable *table;
  TypeSymbol *current;

  Builder(SymbolTable *table);

  void visit(LiteralExpr *expr);
  void visit(BinaryExpr *expr);
  void visit(VarExpr *expr);
  void visit(UnaryExpr *expr);
  void visit(CallExpr *expr);
  void visit(GroupExpr *expr);
  void visit(AssignExpr *expr);
  void visit(AccessExpr *expr);
  void visit(IndexExpr *expr);
  void visit(PostfixExpr *expr);
  void visit(ArrayAccessExpr *expr);
  void visit(TernaryExpr *expr);
  void visit(ThisExpr *expr);
  void visit(SuperExpr *expr);

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

  // util function
  [[noreturn]] void error(const Token &token, const std::string &message) const;
};

class TypeContextGuard {
public:
  TypeContextGuard(TypeSymbol *&current, TypeSymbol *next)
      : current_(current), prev_(current) {
    current_ = next;
  }

  ~TypeContextGuard() { current_ = prev_; }
  TypeContextGuard(const TypeContextGuard &) = delete;
  TypeContextGuard &operator=(const TypeContextGuard &) = delete;

private:
  TypeSymbol *&current_;
  TypeSymbol *prev_;
};

class ScopeGuard {
public:
  ScopeGuard(SymbolTable &t) : table(t) { table.enter(); }
  ~ScopeGuard() { table.exit(); }

  ScopeGuard(const ScopeGuard &) = delete;
  ScopeGuard &operator=(const ScopeGuard &) = delete;

private:
  SymbolTable &table;
};