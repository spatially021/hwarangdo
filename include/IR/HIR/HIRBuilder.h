#pragma once

#include "AST/Decl.h"
#include "AST/Expr.h"
#include "AST/Stmt.h"
#include "AST/Visitor.h"
#include "IR/HIR/HIRDecl.h"
#include "IR/HIR/HIRExpr.h"
#include "IR/HIR/HIRProgram.h"
#include "IR/HIR/HIRStmt.h"
#include "IR/HIR/HIRSymbol.h"
#include "IR/HIR/HIRType.h"
#include "SemanticAnalyzer/SymbolTable.h"
#include "SemanticAnalyzer/symbol/MethodSymbol.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "SemanticAnalyzer/symbol/ValueSymbol.h"
#include "util/Error.h"
#include <cassert>
#include <memory>
#include <utility>

class HIRBuilder : public ASTVisitor {
public:
  explicit HIRBuilder(SymbolTable *table, HIRProgram *program, HIRSource *s);

  void build();

  HIRProgram *program;
  HIRSource *source;

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

  SymbolTable *table;

private:
  HIRType *getOrCreateType(TypeSymbol *symbol);

  void emit(unique_ptr<HIRStmt> stmt);

  // type
  HIRType *lowerType(TypeSymbol *symbol);
  HIREntityType *lowerEntityType(TypeSymbol *symbol);
  HIRStructType *lowerStructType(TypeSymbol *symbol);
  HIREnumType *lowerEnumType(TypeSymbol *symbol);
  HIRHandleType *getOrCreateHandleType(HIREntityType *entity,
                                       StorageKind storage);
  HIRObserverType *getOrCreateObserverType(HIREntityType *entity,
                                           StorageKind storage);

  // decl
  std::unique_ptr<HIRMethodDecl> lowerMethodDecl(FuncDecl *decl);
  HIRLocal *lowerLocal(VarDecl *decl);
  HIRField *lowerField(VarDecl *decl);
  unique_ptr<HIRParam> lowerParam(Param *param);
  std::unique_ptr<HIREnumVariant> lowerEnumVariant(EnumDecl::Variant *variant);

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
  std::unique_ptr<HIRExpr> lowerCall(CallExpr *expr);
  std::unique_ptr<HIRExpr> lowerImplictCall(CallExpr *expr);
  std::unique_ptr<HIRExpr> lowerAssign(AssignExpr *expr);
  std::unique_ptr<HIRExpr> lowerTernary(TernaryExpr *expr);
  std::unique_ptr<HIRExpr> lowerCast(CastExpr *expr);

  // place/value split
  std::unique_ptr<HIRPlaceExpr> lowerPlace(NameExpr *expr);
  std::unique_ptr<HIRExpr> lowerLoadIfNeeded(std::unique_ptr<HIRExpr> expr);
  std::unique_ptr<HIRValueExpr> lowerValue(ValueSymbol *symbol);
  std::unique_ptr<HIRValueExpr> lowerVariantValue(CallExpr *expr);
  std::unique_ptr<HIRValueExpr> lowerVariantValue(MemberExpr *expr);
  std::unique_ptr<HIRFieldPlaceExpr> lowerMember(MemberExpr *expr);
  std::unique_ptr<HIRPlaceExpr> lowerArrayAccess(ArrayAccessExpr *expr);

  // helper
  std::unique_ptr<HIRExpr>
  insertImplicitCastIfNeeded(std::unique_ptr<HIRExpr> expr, HIRType *expected);
  HIRLocal *makeTemp(HIRType *type);
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

    Error::internal(decl->token, "unmatched decl type");
  }

  inline int allocLocalID() {
    if (currentMethod == nullptr) {
      Error::internal("current method is nullptr");
    }
    assert(currentMethod);
    return currentMethod->nextLocalId++;
  }

  inline int allocMethodID() {
    assert(currentType);
    return currentType->nextMethodID++;
  }

  inline int allocParamID() {
    assert(currentMethod);
    return currentMethod->nextParamID++;
  }

  inline int allocFieldID() {
    assert(currentType);
    return currentType->nextFieldId++;
  }

  inline void bindLocal(ValueSymbol *symbol, unique_ptr<HIRLocal> local) {
    assert(currentBlock);
    assert(currentMethod);
    currentBlock->localMap.emplace(symbol, local.get());
    currentMethod->locals.push_back(std::move(local));
  }

  inline void bindField(ValueSymbol *symbol, unique_ptr<HIRField> field) {
    assert(currentType);
    currentType->fieldMap.emplace(symbol, field.get());
    currentType->fields.push_back(std::move(field));
  }

  inline void bindMethod(FuncDecl *decl) {
    auto method = lowerMethodDecl(decl);
    auto raw = method.get();

    auto it = program->typeDeclMap.find(decl->methodSymbol->onwer);

    if (it == program->typeDeclMap.end()) {
      Error::internal(decl->token, "fail to find owner type");
    }

    it->second->methods.push_back(std::move(method));
    currentMethod = raw;
  }

  inline pair<bool, HIRLocal *> lookupLocal(ValueSymbol *symbol) {
    auto it = currentBlock->localMap.find(symbol);
    return {it != currentBlock->localMap.end(), it->second};
  }
  inline pair<bool, HIRParam *> lookupParam(ValueSymbol *symbol) {
    auto it = currentMethod->paramMap.find(symbol);
    return {it != currentMethod->paramMap.end(), it->second};
  }
  inline pair<bool, HIRField *> lookupField(ValueSymbol *symbol) {
    auto it = currentType->fieldMap.find(symbol);
    return {it != currentType->fieldMap.end(), it->second};
  }
  inline pair<bool, HIRField *> lookupField(HIRTypeDecl *type,
                                            ValueSymbol *symbol) {
    auto it = type->fieldMap.find(symbol);
    return {it != type->fieldMap.end(), it->second};
  }

  inline bool isTypeReceiver(Expr *expr) {
    if (auto name = dynamic_cast<NameExpr *>(expr)) {
      return name->resolved->type == Symbol::SymbolType::TYPE;
    }
    return false;
  }

  inline pair<bool, HIREnumVariant *> lookupVariant(EnumVariantSymbol *symbol) {
    auto it = program->variantMap.find(symbol);
    return {it != program->variantMap.end(), it->second};
  }

  inline std::unique_ptr<HIRLoadExpr>
  load(std::unique_ptr<HIRPlaceExpr> place) {
    return make_unique<HIRLoadExpr>(std::move(place));
  }

  inline std::unique_ptr<HIRValueExpr> lowerReceiver(Expr *expr) {
    return lowerValue(expr);
  }
};