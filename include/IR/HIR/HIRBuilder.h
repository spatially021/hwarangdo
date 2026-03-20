#pragma once

#include "AST/Stmt.h"
#include "AST/Visitor.h"
#include "IR/HIR/HIRExpr.h"
#include "IR/HIR/HIRModule.h"
#include "IR/HIR/HIRStmt.h"
#include "SemanticAnalyzer/SymbolTable.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include <memory>

class HIRBuilder : public ASTVisitor {
public:
  explicit HIRBuilder(SymbolTable *table);

  std::unique_ptr<HIRModule> build(/* AST root */);

private:
  HIRModule *module = nullptr;
  HIRMethodDecl *currentMethod = nullptr;
  HIRBlockStmt *currentBlock = nullptr;

  unique_ptr<HIRExpr> exprResult = nullptr;

  int nextLocalId = 0;
  int nextFieldId = 0;
  int nextMethodId = 0;
  int nextVariantId = 0;

private:
  void emit(unique_ptr<HIRStmt> stmt);

  // type
  HIRType *lowerType(TypeSymbol *symbol);
  HIREntityType *lowerEntityType(TypeSymbol *symbol);
  HIRStructType *lowerStructType(TypeSymbol *symbol);
  HIREnumType *lowerEnumType(TypeSymbol *symbol);
  HIRHandleType *getOrCreateHandleType(HIREntityType *entity,
                                       StorageKind storage);
  HIROserverType *getOrCreateObserverType(HIREntityType *entity,
                                          StorageKind storage);

  // decl
  std::unique_ptr<HIRTypeDecl> lowerTypeDecl(/* AST decl */);
  std::unique_ptr<HIRMethodDecl> lowerMethodDecl(/* AST decl */);

  // stmt
  std::unique_ptr<HIRStmt> lowerStmt(/* AST stmt */);
  std::unique_ptr<HIRBlockStmt> lowerBlock(BlockStmt *stmt);
  std::unique_ptr<HIRBlockStmt> lowerStmtAsBlock(Stmt *stmt);
  std::unique_ptr<HIRStmt> lowerSwitch(/* AST switch */);
  std::unique_ptr<HIRStmt> lowerOnExit(/* AST onexit */);

  // expr
  std::unique_ptr<HIRExpr> lowerExpr(Expr *expr);
  std::unique_ptr<HIRExpr> lowerMatch(/* AST match */);
  std::unique_ptr<HIRExpr> lowerSpawn(/* AST spawn */);
  std::unique_ptr<HIRExpr> lowerView(/* AST view */);

  // place/value split
  std::unique_ptr<HIRPlaceExpr> lowerPlace(/* AST expr */);
  std::unique_ptr<HIRExpr> lowerLoadIfNeeded(std::unique_ptr<HIRExpr> expr);

  // helper
  std::unique_ptr<HIRExpr>
  insertImplicitCastIfNeeded(std::unique_ptr<HIRExpr> expr, HIRType *expected);
  HIRLocal *makeTemp(HIRType *type, const std::string &hint);

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
};