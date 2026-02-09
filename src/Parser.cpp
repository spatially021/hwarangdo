#include "Parser.h"
#include "Token.h"
#include "util/Error.h"
#include <memory>
#include <vector>

using ptr = shared_ptr<ASTNode>;

Parser::Parser(const vector<Token> &t) : tokens(t) {}

vector<Stmt::Ptr> Parser::parse() {
  while (!isAtEnd()) {
    Token t = peek();
    auto decl = declaration(TOPLEVEL);
    statements.push_back(make_shared<DeclStmt>(t, decl));
  }
  return statements;
}

Decl::Ptr Parser::declaration(DeclContext context) {
  DeclPrefix prefix = {};
  prefix.startToken = peek();

  ContextGuard _{contexts, context};

  if (isAccessModifier()) {
    prefix.modi = AModifierConvertor(advance());
  }
  if (check(TKind::CONST)) {
    prefix.isConst = true;
    advance();
  }

  if (check(TKind::CLASS)) {
    return classDecl(prefix);
  }

  if (check(TKind::STRUCT)) {
    return structDecl(prefix);
  }

  if (check(TKind::IMPL)) {
    return implDecl(prefix);
  }

  if (check(TKind::TRAIT)) {
    return traitDecl(prefix);
  }

  if (check(TKind::ENUM)) {
    return enumDecl(prefix);
  }

  if (check(TKind::FUNC)) {
    return functionDecl(prefix, true);
  }

  if (check(TKind::VOID)) {
    return functionDecl(prefix);
  }

  if (isType()) {
    if (isFunc()) {
      return functionDecl(prefix);
    } else {
      return varDecl(prefix);
    }
  }

  Error::diagnostic(peek(), "only declaration in top-level");
}

Stmt::Ptr Parser::statement() {
  switch (peek().kind) {
  case TKind::SEMICOLON:
    return make_shared<EmptyStmt>(advance());

  case TKind::IF:
    return ifStmt();

  case TKind::SWITCH:
    return switchStmt();
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

  case TKind::THROW:
    return throwStmt();

  case TKind::IDENTIFIER:
    if (following().kind == TKind::IDENTIFIER)
      return declStmt();
    else
      return expressionStmt();
  default:
    return expressionStmt();
  }
}

Expr::Ptr Parser::expression() {
  Expr::Ptr expr = assignment();
  return expr;
}
