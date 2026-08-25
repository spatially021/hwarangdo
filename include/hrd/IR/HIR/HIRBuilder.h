#pragma once

#include "hrd/AST/Decl.h"
#include "hrd/AST/Expr.h"
#include "hrd/AST/Stmt.h"
#include "hrd/AST/Visitor.h"
#include "hrd/IR/HIR/HIRDecl.h"
#include "hrd/IR/HIR/HIRExpr.h"
#include "hrd/IR/HIR/HIRProgram.h"
#include "hrd/IR/HIR/HIRStmt.h"
#include "hrd/IR/HIR/HIRSymbol.h"
#include "hrd/Recover/HIRReover.h"
#include "hrd/SemanticAnalyzer/SymbolTable/SymbolTable.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/compiler/CompilerContexts.h"
#include "hrd/diagnostic/DiagnosticEngine.h"
#include "hrd/util/Error.h"
#include <cassert>
#include <memory>
#include <utility>

class HIRBuilder : public ASTVisitor {
public:
  explicit HIRBuilder(HIRContext &context, HIRSource *s);

  void build();

  HIRProgram *program;
  HIRSource *source;
  DiagnosticEngine &engine;
  HIRRecover recover;

private:
  HIRProgram *module = nullptr;
  HIRMethodDecl *currentMethod = nullptr;
  HIRBlockStmt *currentBlock = nullptr;
  HIRTypeDecl *currentType = nullptr;

  bool isField = false;

  unique_ptr<HIRExpr> exprResult = nullptr;
  unique_ptr<HIRStmt> declResult = nullptr;

  int nextMethodId = 0;
  int nextVariantId = 0;

  SymbolTable &table;

private:
  void emit(unique_ptr<HIRStmt> stmt);
  void setDefaultInit(HIRTypeDecl *typeDecl);

  // type
  TypeSymbol *lowerStructType(TypeSymbol *symbol);
  TypeSymbol *lowerEnumType(TypeSymbol *symbol);

  // decl
  HIRLocal *lowerLocal(VarDecl *decl);
  unique_ptr<HIRParam> lowerParam(Param *param);

  // stmt
  std::unique_ptr<HIRBlockStmt> lowerBlock(BlockStmt *stmt);
  std::unique_ptr<HIRBlockStmt> lowerStmtAsBlock(Stmt *stmt);
  std::unique_ptr<HIRStmt> lowerSwitch(SwitchStmt *stmt);
  std::unique_ptr<HIRCase> lowerCase(Case *stmt);
  std::unique_ptr<HIRStmt> lowerFor(ForStmt *stmt);
  std::unique_ptr<HIRStmt> lowerIf(IfStmt *stmt);
  std::unique_ptr<HIRStmt> lowerWhile(WhileStmt *stmt);
  std::unique_ptr<HIRStmt> lowerReturn(ReturnStmt *stmt);
  std::unique_ptr<HIRStmt> lowerValueTransfer(ValueTransferStmt *stmt);
  std::unique_ptr<HIRStmt> lowerExprStmt(ExprStmt *stmt);
  std::unique_ptr<HIRStmt> lowerDestroyStmt(DestroyExpr *expr);
  std::unique_ptr<HIRStmt> lowerQuitStmt(QuitExpr *expr);
  std::unique_ptr<HIRStmt> lowerAssign(AssignExpr *expr);

  // expr
  std::unique_ptr<HIRExpr> lowerExpr(Expr *expr);
  std::unique_ptr<HIRExpr> lowerMatch(MatchExpr *expr);
  std::unique_ptr<HIRExpr> lowerSpawn(SpawnExpr *expr);
  std::unique_ptr<HIRExpr> lowerView(ViewExpr *expr);
  std::unique_ptr<HIRExpr> lowerCall(CallExpr *expr);
  std::unique_ptr<HIRExpr> lowerInitCall(CallExpr *expr);
  std::unique_ptr<HIRExpr> lowerImplictCall(CallExpr *expr);
  std::unique_ptr<HIRExpr> lowerTernary(TernaryExpr *expr);
  std::unique_ptr<HIRExpr> lowerCast(CastExpr *expr);
  std::unique_ptr<HIRExpr> lowerLiteral(LiteralExpr *expr);
  std::unique_ptr<HIRExpr> lowerRuntime(CallExpr *expr);

  // place/value split
  std::unique_ptr<HIRPlaceExpr> lowerPlace(NameExpr *expr);
  std::unique_ptr<HIRExpr> lowerLoadIfNeeded(std::unique_ptr<HIRExpr> expr);
  std::unique_ptr<HIRValueExpr> lowerVariantValue(CallExpr *expr);
  std::unique_ptr<HIRValueExpr> lowerVariantValue(MemberExpr *expr);
  std::unique_ptr<HIRFieldPlaceExpr> lowerMember(MemberExpr *expr);
  std::unique_ptr<HIRPlaceExpr> lowerArrayAccess(ArrayAccessExpr *expr);
  std::unique_ptr<HIRValueExpr> lowerCallArg(Expr *arg, ParamSymbol *param);
  std::unique_ptr<HIRCasePattern> lowerCaseValue(CaseValueExpr *epxr);

  // helper
  std::unique_ptr<HIRExpr>
  insertImplicitCastIfNeeded(std::unique_ptr<HIRExpr> expr,
                             TypeSymbol *expected);
  // HIRLocal *makeTemp(HIRType *type);
  HIRLocal *lookUpLocal(ValueSymbol *);
  unique_ptr<HIRValueExpr> lowerValue(Expr *expr);
  std::unique_ptr<HIRSelfExpr> lowerImplictSelf();

private:
#define AST_NODE(T) void visit(T *node) override;
#include "../../AST/ASTNodeList.def"
#undef AST_NODE

  // helper inlines
private:
  inline TypeSymbol *getTypeSymbolFromDecl(Decl *decl, HIRTypeDeclKind &kind) {
    if (auto *c = dynamic_cast<ClassDecl *>(decl)) {
      kind = HIRTypeDeclKind::Class;
      return c->symbol;
    }
    if (auto *e = dynamic_cast<EnumDecl *>(decl)) {
      kind = HIRTypeDeclKind::Enum;
      return e->symbol;
    }
    if (auto *s = dynamic_cast<StructDecl *>(decl)) {
      kind = HIRTypeDeclKind::Struct;
      return s->symbol;
    }
    if (dynamic_cast<TraitDecl *>(decl)) {
      return nullptr;
    }

    Error::internal(decl->span, "unmatched decl type");
  }

  int allocLocalID();
  int allocMethodID();
  int allocParamID();
  int allocFieldID();
  int allocRootID();

  void bindLocal(ValueSymbol *symbol, unique_ptr<HIRLocal> local);
  void bindMethod(FuncDecl *decl);

  pair<bool, HIRLocal *> lookupLocal(ValueSymbol *symbol);
  pair<bool, HIRParam *> lookupParam(ValueSymbol *symbol);
  pair<bool, HIRMethodDecl *> lookupMethod(HIRTypeDecl *decl,
                                           MethodSymbol *symbol);
  pair<bool, HIRMethodDecl *> lookupInit(HIRTypeDecl *deck,
                                         MethodSymbol *symbol);

  bool isTypeReceiver(Expr *expr);

  inline std::unique_ptr<HIRLoadExpr>
  load(std::unique_ptr<HIRPlaceExpr> place) {
    return make_unique<HIRLoadExpr>(place->span, std::move(place));
  }

  std::unique_ptr<HIRValueExpr> lowerReceiver(Expr *expr) {
    return lowerValue(expr);
  }
};