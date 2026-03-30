#pragma once

#include "AST/ASTNode.h"
#include "AST/Expr.h"
#include "AST/Visitor.h"
#include "SemanticAnalyzer/ResolvedLit.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "SemanticAnalyzer/symbol/ValueSymbol.h"
#include "SymbolTable.h"
#include <cstddef>

class SymbolTable;

class Resolver : public ASTVisitor {
  using str = string const &;

public:
  SymbolTable *table = nullptr;
  TypeSymbol *currentType = nullptr;
  MethodSymbol *currentMethod = nullptr;
  ASTNode *currentSwitch = nullptr;
  Case *currentCase = nullptr;

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
  // util function

  ValueSymbol *resolveValue(str name);
  ValueSymbol *lookLocalValue(str name, Scope *localScope);

  ResolvedLit resolveLitInt(LiteralExpr *expr);
  ResolvedLit resolveLitFloat(LiteralExpr *expr);
  ResolvedLit resolveChar(LiteralExpr *expr);
  ResolvedLit resolveString(LiteralExpr *expr);

private:
  Scope *currentSelf = nullptr;
  Scope *currentBase = nullptr;

  void ResolveEnumVariant(CallExpr *expr);
  void ResolveCall(CallExpr *expr);
  bool isAssignable(TypeSymbol *from, TypeSymbol *to);
  bool isBinaryOperatalbe(Operator op, TypeSymbol *left, TypeSymbol *right);

  bool isCmpable(TypeSymbol *left, TypeSymbol *right);
  TypeSymbol *binaryResult(Operator op, TypeSymbol *left, TypeSymbol *right);
  bool isCastable(TypeSymbol *from, TypeSymbol *to);
  TypeSymbol *binaryCasting(TypeSymbol *from, TypeSymbol *to);
  [[noreturn]]
  void unmatchSymbol(Symbol *symbol);
  vector<TypeSymbol *> getPromotionCandidates(TypeSymbol *left,
                                              TypeSymbol *right);
  bool canImplicitlyConvert(TypeSymbol *from, TypeSymbol *to);
  TypeSymbol *implicitCasting(TypeSymbol *from, TypeSymbol *to);

  inline bool isValidUnicodeScalar(uint32_t cp) {
    if (cp > 0x10FFFF)
      return false;
    if (cp >= 0xD800 && cp <= 0xDFFF)
      return false;
    return true;
  }

  inline int classifyCharWidth(uint32_t cp) {
    if (!isValidUnicodeScalar(cp))
      return 0;
    if (cp <= 0xFF)
      return 8;
    if (cp <= 0xFFFF)
      return 16;
    return 32;
  }

  inline int classifyStringWidth(const vector<uint32_t> &cps) {
    uint32_t maxCp = 0;
    for (auto cp : cps) {
      if (!isValidUnicodeScalar(cp))
        return 0;
      if (cp > maxCp)
        maxCp = cp;
    }

    if (maxCp <= 0xFF)
      return 8;
    if (maxCp <= 0xFFFF)
      return 16;
    return 32;
  }

  static bool isHexDigit(char c) {
    return std::isdigit(static_cast<unsigned char>(c)) ||
           (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
  }

  static uint32_t hexValue(char c) {
    if (c >= '0' && c <= '9')
      return static_cast<uint32_t>(c - '0');
    if (c >= 'a' && c <= 'f')
      return static_cast<uint32_t>(10 + (c - 'a'));
    if (c >= 'A' && c <= 'F')
      return static_cast<uint32_t>(10 + (c - 'A'));
    throw std::runtime_error("invalid hex digit");
  }

  static uint32_t parseHex(const std::string &s, size_t start, size_t count) {
    uint32_t value = 0;
    for (size_t i = 0; i < count; ++i) {
      char c = s[start + i];
      if (!isHexDigit(c)) {
        throw std::runtime_error("invalid hex digit in unicode escape");
      }
      value = (value << 4) | hexValue(c);
    }
    return value;
  }

  // token.text 기준:
  // "a"
  // "\\n"
  // "\\uAC00"
  // "\\U0001F600"
  uint32_t decodeCharLiteral(const Token &token, str s);
  uint32_t decodeOneUtf8CodePoint(const Token &token, str s, size_t &i);

  bool fitsFloatRange(const llvm::APFloat &base, const llvm::fltSemantics &sem);
  llvm::APFloat convertFloatTo(const llvm::APFloat &base,
                               const llvm::fltSemantics &sem);

  inline bool canPlaceView(Expr::Ptr expr) {
    if (expr->kind == NKind::SPAWN_EXPR || expr->kind == NKind::ASSIGN_EXPR) {
      return false;
    }
    return true;
  }

  inline bool isLit(Expr::Ptr expr) {
    auto t = expr->resolvedType;
    return table->isInt(t) || table->isFloat(t) || table->isBool(t),
           table->isFixed(t) || table->isString(t) || table->isChar(t);
  }

  ValueSymbol *lookupEnumVariant(TypeSymbol *enumType, const string &name,
                                 Token token);
  TypeSymbol *getTargetType();
};
