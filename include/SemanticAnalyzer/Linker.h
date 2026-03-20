#pragma once

#include "AST/Visitor.h"
#include "SemanticAnalyzer/SymbolTable.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"

class Linker : public ASTVisitor {

private:
  SymbolTable *table = nullptr;
  TypeSymbol *currentType = nullptr;

public:
  Linker(SymbolTable *t);

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

  void visit(FuncDecl *decl);
  void visit(VarDecl *decl);
  void visit(ArrayDecl *decl);

  void visit(TypeNode *decl);
  void visit(ASTNode *node);

  void visit(TraitSig *sig);
  void visit(Param *param);
  void visit(InitDecl *decl);

private:
  inline bool isDeclField(TypeSymbol *type) {
    return (type->kind == TypeSymbol::TypeKind::PRIMITIVE ||
            type->kind == TypeSymbol::TypeKind::HANDLE ||
            type->kind == TypeSymbol::TypeKind::STRUCT ||
            type->kind == TypeSymbol::TypeKind::ENUM);
  }
};