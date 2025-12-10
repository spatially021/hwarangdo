#pragma once

#include "AST/Decl.h"
#include "Token.h"
#include <memory>
#include <stdexcept>
#include <unordered_set>
#include <vector>

class Parser {
public:
  explicit Parser(const std::vector<Token> &tokens);
  std::vector<Stmt::Ptr> statements;
  // 최상위 파싱 엔트리
  std::vector<Stmt::Ptr> parse();
  std::unordered_set<string> types;
  shared_ptr<Program> program;

private:
  enum ExprType { LOGICAL_EXPR, ARITHMETIC_EXPR, STRING_EXPR };

  const std::vector<Token> &tokens;
  size_t current = 0;

  // 문장 단위
  Stmt::Ptr declaration(); // 변수 선언, 함수 선언 등
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
  Expr::Ptr unary();
  Expr::Ptr primary();
  Expr::Ptr postfix();
  Expr::Ptr ternary();
  Expr::Ptr postfixAfterMember(Expr::Ptr expr);
  Expr::Ptr conditionExpr();
  Expr::Ptr varExpr(Token before);

  // 함수 및 블록
  Stmt::Ptr ifStmt();
  Stmt::Ptr bodyStmt();
  Stmt::Ptr forStmt();
  Stmt::Ptr whileStmt();
  Stmt::Ptr expressionStmt();
  Stmt::Ptr returnStmt();
  Stmt::Ptr switchStmt();
  Stmt::Ptr caseStmt();

  Stmt::Ptr functionDecl(bool isDynamic = false);
  Stmt::Ptr varDecl();
  Stmt::Ptr block();
  Stmt::Ptr classDecl();

  // 유틸리티
  bool match(std::initializer_list<TokKind> kinds);
  bool check(TokKind kind) const;
  const Token &advance();
  const Token &peek() const;
  const Token &previous() const;
  bool isAtEnd() const;
  const Token &consume(TokKind kind, const std::string &message);
  void error(const Token &token, const std::string &message);
  bool isFunc() const;
  bool isType() const;
  Token parseLiteralForType(const Token &type);
  bool isValidSize(const std::string &s) const;
};
