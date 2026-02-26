#pragma once

#include "AST/Visitor.h"
#include "Symbol.h"
#include "SymbolTable.h"

class SymbolTable;

class Resolver : public ASTVisitor {
  using str = string const &;

public:
  SymbolTable *table = nullptr;
  TypeSymbol *currentType = nullptr;
  MethodSymbol *currentMethod = nullptr;

  Resolver(SymbolTable *table);

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
  void visit(MoveExpr *expr);
  void visit(BorrowExpr *expr);
  void visit(ReferenceExpr *expr);

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

  void visit(FuncDecl *decl);
  void visit(VarDecl *decl);
  void visit(ArrayDecl *decl);

  void visit(TypeNode *decl);
  void visit(ASTNode *node);

  void visit(TraitSig *sig);
  void visit(Param *param);

  // util function

  ValueSymbol *resolveValue(str name);
  ValueSymbol *lookLocalValue(str name, Scope *localScope);

private:
  Scope *currentSelf = nullptr;
  Scope *currentBase = nullptr;

  void ResolveEnumVariant(CallExpr *expr);
  void ResolveCall(CallExpr *expr);
  bool isAssignable(TypeSymbol *from, TypeSymbol *to);
  bool isBinaryOperatalbe(BinaryExpr::OperatorType op, TypeSymbol *left,
                          TypeSymbol *right);

  bool isCmpable(TypeSymbol *left, TypeSymbol *right);
  TypeSymbol *binaryResult(BinaryExpr::OperatorType op, TypeSymbol *left,
                           TypeSymbol *right);
  bool isCastable(TypeSymbol *from, TypeSymbol *to);
  TypeSymbol *casting(TypeSymbol *from, TypeSymbol *to);
  [[noreturn]]
  void unmatchSymbol(Symbol *symbol);
};