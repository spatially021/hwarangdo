#pragma once

#include "include/Parser.h"
#include "include/AST/Decl.h"
#include "include/AST/Stmt.h"
#include "include/Token.h"
#include <memory>
#include <vector>

using Ptr = shared_ptr<ASTNode>;

Parser::Parser(const vector<Token> &tokens) : tokens(tokens) {}

vector<Stmt::Ptr> Parser::parse() {
  while (!isAtEnd()) {
    if (check({TKind::CLASS, TKind::STRUCT, TKind::IMPL, TKind::TRAIT})) {
      auto stmt = statement();
      statements.push_back(stmt);
    } else if (isAccessModifier()) {
      if (following().kind == TKind::CLASS ||
          following().kind == TKind::STRUCT ||
          following().kind == TKind::IMPL || following().kind == TKind::TRAIT) {
        auto stmt = statement();
        statements.push_back(stmt);
      } else {
        error(peek(), "not allowed expression");
      }
    } else {
      error(peek(), "not allowed expression");
    }
  }

  return statements;
}

Decl::Ptr Parser::declaration() {
  if (isAccessModifier()) {

    if (following().kind == TKind::CONST) {
      if (isType() && !isFunc())
        return varDecl();
      else
        error(following(), "invalid expression of const.");
    }

    if (check(TKind::CLASS, 1)) {
      return classDecl();
    }

    if (check(TKind::STRUCT, 1)) {
      return structDecl();
    }

    if (check(TKind::IMPL), 1)
      return implDecl();

    if (check(TKind::TRAIT), 1)
      return traitDecl();

    if (isType()) {
      if (isFunc())
        return functionDecl();
      else
        return varDecl();
    }

    if (check(TKind::VOID))
      return functionDecl();
    if (check(TKind::FUNC))
      return functionDecl(true);

    error(following(), "expect declaration after access modifier.");
  } else {

    if (following().kind == TKind::CONST) {
      if (isType() && !isFunc())
        return varDecl();
      else
        error(following(), "invalid expression of const.");
    }
    if (check(TKind::CLASS))
      return classDecl();

    if (check(TKind::STRUCT))
      return structDecl();

    if (check(TKind::IMPL))
      return implDecl();

    if (check(TKind::TRAIT))
      return traitDecl();

    if (isType()) {
      if (isFunc())
        return functionDecl();
      else
        return varDecl();
    }

    if (check(TKind::VOID))
      return functionDecl();
    if (check(TKind::FUNC))
      return functionDecl(true);
  }
  error(peek(), "Only declarations are allowed here.");
}

Stmt::Ptr Parser::statement() {
  switch (peek().kind) {
  case TKind::SEMICOLON:
    return make_shared<EmptyStmt>(advance());

  case TKind::IF:
    return ifStmt();

  case TKind::SWITCH:
    return switchStmt();
  case TKind::CASE:
  case TKind::DEFAULT:
    return caseStmt();
  case TKind::FOR:
    return forStmt();
  case TKind::WHILE:
    return whileStmt();
  case TKind::BREAK:
    return make_shared<BreakStmt>(advance());
  case TKind::CONTINUE:
    return make_shared<ContinueStmt>(advance());
  case TKind::RETURN:
    return returnStmt();

  case TKind::INT:
  case TKind::FLOAT:
  case TKind::STRING:
  case TKind::CHAR:
  case TKind::FIXED:
  case TKind::BOOL:
  case TKind::FUNC:
  case TKind::VOID:
  case TKind::CLASS:
  case TKind::STRUCT:
  case TKind::ENUM:
  case TKind::PUBLIC:
  case TKind::PROTECTED:
  case TKind::PRIVATE:
  case TKind::IMPL:
  case TKind::TRAIT:
    return declStmt();

  case TKind::TRY:
    return tryStmt();

  case TKind::ONEXIT:
    return onexitStmt();
  default:
    return expressionStmt();
  }
}

Expr::Ptr Parser::expression() {}