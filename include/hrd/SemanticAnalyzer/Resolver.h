#pragma once

#include "hrd/AST/ASTNode.h"
#include "hrd/AST/Expr.h"
#include "hrd/AST/Visitor.h"
#include "hrd/Recover/ResolverRecover.h"
#include "hrd/SemanticAnalyzer/MethodBucket.h"
#include "hrd/SemanticAnalyzer/ResolvedLit.h"
#include "hrd/SemanticAnalyzer/Scope.h"
#include "hrd/SemanticAnalyzer/SymbolTable/SymbolTable.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/Symbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/SourceSpan.h"
#include "hrd/compiler/CompilerContexts.h"
#include "hrd/diagnostic/DiagnosticEngine.h"
#include "hrd/enums/Argument.h"
#include "hrd/enums/Casting.h"
#include "hrd/util/Error.h"
#include "hrd/util/TypeResolver.h"
#include <cassert>
#include <cstddef>
#include <vector>

class SymbolTable;

class Resolver : public ASTVisitor {
  using str = string const &;

public:
  SymbolTable &table;
  TypeSymbol *&currentType;
  MethodSymbol *currentMethod = nullptr;
  ASTNode *currentSwitch = nullptr;
  Case *currentCase = nullptr;
  DiagnosticEngine &engine;
  ResolverRecover recover;
  TypeResolverContext typeContext;

  Resolver(ResolverContext &context);
#define AST_NODE(T) void visit(T *node) override;
#include "../AST/ASTNodeList.def"
#undef AST_NODE

  // util function

  ValueSymbol *resolveValue(str name);
  ValueSymbol *lookLocalValue(str name, Scope *localScope);

  ResolvedLit resolveLitFloat(LiteralExpr *expr);
  ResolvedLit resolveChar(LiteralExpr *expr);
  ResolvedLit resolveString(LiteralExpr *expr);

private:
  Scope *currentSelf = nullptr;
  Scope *currentBase = nullptr;
  bool tryResolveRuntime(CallExpr *expr);
  void ResolveEnumVariant(CallExpr *expr);
  void resolveCall(CallExpr *expr, TypeSymbol *scope, bool isImplict = false);
  void resolveInit(CallExpr *expr);
  void resolveCasePayload(CallExpr *expr);
  void ResolveStaticMethod(CallExpr *expr, TypeSymbol *scope);
  ArgMatchKind matchArgument(Expr *arg, TypeSymbol *param,
                             bool hasInit = false);
  int rankOf(const ArgMatchKind &kind);

  bool isBetterThan(const vector<ArgMatchKind> &a,
                    const vector<ArgMatchKind> &b);

  bool isAssignable(TypeSymbol *from, TypeSymbol *to);
  bool isBinaryOperatalbe(Operator op, TypeSymbol *left, TypeSymbol *right);
  bool isCmpable(TypeSymbol *left, TypeSymbol *right);
  pair<TypeSymbol *, CastingResultKind>
  binaryResult(Operator op, TypeSymbol *left, TypeSymbol *right);
  bool isCastable(TypeSymbol *from, TypeSymbol *to);
  pair<TypeSymbol *, CastingResultKind> binaryCasting(TypeSymbol *from,
                                                      TypeSymbol *to);
  [[noreturn]]
  void unmatchSymbol(Symbol *symbol);
  vector<TypeSymbol *> getPromotionCandidates(TypeSymbol *left,
                                              TypeSymbol *right);

  // pair<bool, CastingResultKind> canImplicitlyConvert(TypeSymbol *from,
  //                                                    TypeSymbol *to);
  pair<bool, CastingResultKind> canImplicitlyLiteralConvert(LiteralExpr *from,
                                                            TypeSymbol *to);
  pair<TypeSymbol *, CastingResultKind> implicitCasting(Expr *from,
                                                        TypeSymbol *to);

  void castFail(CastingResultKind kind, SourceSpan &span);

  void inferencePrim(TypeNode *decl, TypeSymbol *expr);
  void convertLit(LiteralExpr *lit, TypeNode *type);
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

    Error::internal("invalid hex digit");
  }

  static uint32_t parseHex(const std::string &s, size_t start, size_t count) {
    uint32_t value = 0;
    for (size_t i = 0; i < count; ++i) {
      char c = s[start + i];
      if (!isHexDigit(c)) {
        Error::internal("invalid hex digit in unicode escape");
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
  uint32_t decodeCharLiteral(const SourceSpan &token, str s);
  uint32_t decodeOneUtf8CodePoint(const SourceSpan &token, str s, size_t &i);

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
    return isa<IntType>(t) || isa<FloatType>(t) || isa<BoolType>(t) ||
           isa<StringType>(t) || isa<CharType>(t);
  }

  ValueSymbol *lookupEnumVariant(EnumType *enumType, const string &name,
                                 SourceSpan &token);
  EnumType *getTargetType();

  inline bool isTypeReceiver(Expr *expr) {
    if (auto name = dynamic_cast<NameExpr *>(expr)) {
      return name->resolved->type == Symbol::SymbolType::TYPE;
    }
    return false;
  }
  llvm::APInt resolveFixedArraySize(Expr *expr);

  // pair<bool, MethodSymbol *> lookupMethod(str name, Scope *scope,
  //                                         vector<TypeSymbol *> args);
  pair<bool, MethodSymbol *> lookupInit(ObjectType *type,
                                        vector<TypeSymbol *> args);

private:
  MethodSymbol *resolveMethodOverload(SourceSpan span,
                                      const vector<MethodSymbol *> &bucket,
                                      const vector<Expr *> &args);
  RuntimeSymbol *resolveRuntimeOverload(SourceSpan span,
                                        const vector<RuntimeSymbol *> &bucket,
                                        const vector<Expr *> &args);
  template <typename SymbolT, typename ParamCount, typename ParamType,
            typename HasDefault>
  SymbolT *
  resolveOverload(SourceSpan span, const std::vector<SymbolT *> &bucket,
                  const std::vector<Expr *> &args, ParamCount paramCount,
                  ParamType paramType, HasDefault hasDefault,
                  std::string_view unknownMessage,
                  std::string_view ambiguousMessage);

  void checkMethodAccess(CallExpr *expr, MethodSymbol *method, bool isImplicit);

  MethodBucket getMethodBucket(str name, TypeSymbol *scope, bool isStatic);
};
