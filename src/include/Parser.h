#pragma once

#include "AST/ASTNode.h"
#include "AST/Decl.h"
#include "AST/Stmt.h"
#include "Token.h"
#include <cassert>
#include <cstddef>
#include <memory>
#include <unordered_set>
#include <vector>

using namespace std;

enum DeclContext {
  TOPLEVEL,
  BLOCK,
  CLASSBODY,
  IMPLBODY,
};

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

  ContextGuard(const ContextGuard&)=delete;
  ContextGuard &operator=(const ContextGuard &)=delete;

private:
  std::vector<DeclContext> &stack_;
};

struct DeclPrefix {
  AModifier modi = AModifier::DEFAULT;
  bool isConst = false;
  Token startToken;
};

class Parser{
public:
explicit Parser(const vector<Token> &tokens);
vector<Stmt::Ptr> statements;
vector<Stmt::Ptr> parse();
vector<DeclContext> contexts;

private:

const std::vector<Token> &tokens;
size_t current = 0;

// 문장 단위
Decl::Ptr declaration(DeclContext context); // 변수 선언, 함수 선언 등
Stmt::Ptr statement();   // 일반 문장 (if, while, return, expression 등)

// 표현식 단위
Expr::Ptr expression();

Expr::Ptr assignment();
Expr::Ptr logicalOr();
Expr::Ptr logicalAnd();
Expr::Ptr equality();
Expr::Ptr comparison();
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

Decl::Ptr classDecl(DeclPrefix prefix);
Decl::Ptr structDecl(DeclPrefix prefix);
Decl::Ptr implDecl(DeclPrefix prefix);
Decl::Ptr traitDecl(DeclPrefix prefix);
Decl::Ptr enumDecl(DeclPrefix prefix);
Decl::Ptr functionDecl(DeclPrefix prefix,bool isDynamic = false);
Decl::Ptr varDecl(DeclPrefix prefix);

// 유틸리티
bool match(std::initializer_list<TKind> kinds);
bool check(TKind kind, size_t step = 0) const;
bool check(std::initializer_list<TKind> kind, size_t step = 0) const;
const Token &advance();
const Token &peek() const;
const Token &previous() const;
const Token &following(size_t step = 1) const;
bool isAtEnd() const;
const Token &consume(TKind kind, const std::string &message);
[[noreturn]]
void error(const Token &token, const std::string &message) const;
bool isFunc() const;
bool isType() const;
Token parseLiteralForType(const Token &type);
bool isValidSize(const std::string &s) const;
bool isAccessModifier() const;
bool isTypeToken(TKind k) const ;
AModifier AModifierConvertor(Token t);
TypeNode::Ptr typeNodeConvertor(Token t);
bool isAssign() const;
bool isAssginable(Expr::Ptr p) const;
};