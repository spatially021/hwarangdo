#pragma once

#include "AST/ASTNode.h"
#include "AST/Decl.h"
#include "AST/DeclContext.h"
#include "AST/Expr.h"
#include "AST/Stmt.h"
#include "Token.h"
#include "enums/AccessModifier.h"
#include "hrd/compiler/CompilerContexts.h"
#include "hrd/util/diagnostic/Diagnostic.h"
#include "hrd/util/diagnostic/DiagnosticEngine.h"
#include <cassert>
#include <cstddef>
#include <memory>
#include <vector>

using namespace std;

class Error;

class ContextGuard {
public:
  ContextGuard(std::vector<DeclContext> &stack, DeclContext ctx)
      : stack_(stack) {
    stack_.push_back(ctx);
  }

  ~ContextGuard() {
    assert(!stack_.empty());
    stack_.pop_back();
  }

  ContextGuard(const ContextGuard &) = delete;
  ContextGuard &operator=(const ContextGuard &) = delete;

private:
  std::vector<DeclContext> &stack_;
};

struct DeclPrefix {
  AModifier modi = AModifier::PUBLIC;
  Token startToken;
  bool isExtern = false;
  bool isConst = false;
  bool isRoot = false;
  bool isFrame = false;
  bool isOverride = false;
  bool isAsync = false;
};

class Parser {
public:
  explicit Parser(ParserContext &context);
  vector<Stmt::Ptr> statements;
  vector<Decl::Ptr> parse();
  vector<DeclContext> contexts;

private:
  const std::vector<Token> &tokens;
  DiagnosticEngine &engine;
  size_t current = 0;

  // 문장 단위
  Decl::Ptr declaration(DeclContext context); // 변수 선언, 함수 선언 등
  Stmt::Ptr statement(); // 일반 문장 (if, while, return, expression 등)

  // 표현식 단위
  Expr::Ptr expression();

  Expr::Ptr assignment();
  Expr::Ptr logicalOr();
  Expr::Ptr logicalAnd();
  Expr::Ptr bitOr();
  Expr::Ptr bitXor();
  Expr::Ptr bitAnd();
  Expr::Ptr equality();
  Expr::Ptr comparison();
  Expr::Ptr shift();
  Expr::Ptr term();
  Expr::Ptr factor();
  Expr::Ptr power();
  Expr::Ptr unary();
  Expr::Ptr primary();
  Expr::Ptr postfix();
  Expr::Ptr ternary();

  // 함수 및 블록
  Stmt::Ptr expressionStmt();
  Stmt::Ptr declStmt();
  Stmt::Ptr blockStmt();
  Stmt::Ptr bodyStmt();
  Stmt::Ptr ifStmt();
  Stmt::Ptr forStmt();
  Stmt::Ptr whileStmt();
  Stmt::Ptr switchStmt();
  Stmt::Ptr returnStmt();
  Stmt::Ptr tryStmt();
  Stmt::Ptr catchStmt();
  Stmt::Ptr onexitStmt();
  Stmt::Ptr throwStmt();
  Stmt::Ptr valueTransferStmt();
  shared_ptr<Case> caseStmt(bool isSwtich = true);

  Decl::Ptr classDecl(DeclPrefix prefix);
  Decl::Ptr structDecl(DeclPrefix prefix);
  Decl::Ptr implDecl(DeclPrefix prefix);
  Decl::Ptr traitDecl(DeclPrefix prefix);
  Decl::Ptr enumDecl(DeclPrefix prefix);
  Decl::Ptr functionDecl(DeclPrefix prefix, bool isDynamic = false);
  Decl::Ptr varDecl(DeclPrefix prefix);
  Decl::Ptr handleDecl(DeclPrefix prefix);
  Decl::Ptr initDecl(DeclPrefix prefix);
  Decl::Ptr onDestroyDecl(DeclPrefix prefix);

  // 유틸리티
  bool match(std::initializer_list<TKind> kinds);
  bool check(TKind kind, size_t step = 0) const;
  bool check(std::initializer_list<TKind> kind, size_t step = 0) const;
  const Token &advance();
  const Token &peek() const;
  const Token &previous() const;
  const Token &following(size_t step = 1) const;
  bool isAtEnd() const;
  const Token &consume(TKind kind, DiagnosticCode code,
                       const std::string &message);
  bool isFunc() const;
  bool isInit() const;
  bool isType() const;
  Token parseLiteralForType(const Token &type);
  bool isValidSize(const std::string &s) const;
  bool isAccessModifier() const;
  bool isTypeToken(TKind k) const;
  AModifier AModifierConvertor(Token t);
  TypeNode::Ptr typeNodeConvertor(Token t, Token size = {});
  bool isAssign() const;
  bool isAssginable(Expr::Ptr p) const;

  TypeNode::Ptr parseType();

  Expr::Ptr parseCaseValue();

private:
  void notFunc(DeclPrefix prefix);
  void notVar(DeclPrefix prefix);

  inline bool isLit() {
    return check({TKind::LIT_BOOL, TKind::LIT_INT, TKind::LIT_CHARACTER,
                  TKind::LIT_STRING, TKind::LIT_FLOAT});
  }
};