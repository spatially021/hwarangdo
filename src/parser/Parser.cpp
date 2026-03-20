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
  while (check({
      TKind::CONST,
      TKind::ROOT,
      TKind::FRAME,
      TKind::OVERRIDE,
      TKind::ASYNC,
  })) {
    if (check(TKind::CONST)) {
      advance();
      if (prefix.isConst)
        Error::diagnostic(peek(), "duplicate const modifier");
      prefix.isConst = true;
    }
    if (check(TKind::ROOT)) {
      advance();
      if (prefix.isRoot)
        Error::diagnostic(peek(), "duplicate root modifier");
      prefix.isRoot = true;
    }
    if (check(TKind::FRAME)) {
      advance();
      if (prefix.isFrame) {
        Error::diagnostic(peek(), "duplicate frame modifier");
      }
      prefix.isFrame = true;
    }
    if (check(TKind::OVERRIDE)) {
      advance();
      if (prefix.isOverride) {
        Error::diagnostic(peek(), "duplicate override modifier");
      }
      prefix.isOverride = true;
    }
    if (check(TKind::ASYNC)) {
      advance();
      if (prefix.isAsync) {
        Error::diagnostic(peek(), "duplicated async modifier");
      }
      prefix.isAsync = true;
    }
  }
  if (check(TKind::FUNC)) {
    return functionDecl(prefix, true);
  }

  if (check(TKind::VOID)) {
    return functionDecl(prefix);
  }

  if (check(TKind::INIT)) {
    return initDecl(prefix);
  }

  if (isType()) {
    if (isFunc()) {
      return functionDecl(prefix);
    } else {
      return varDecl(prefix);
    }
  }

  if (check(TKind::HANDLE)) {
    return handleDecl(prefix);
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
  case TKind::CONST:
  case TKind::ROOT:
  case TKind::HANDLE:
  case TKind::FRAME:
  case TKind::INIT:
    return declStmt();

  case TKind::DOUBLE_ANGLEBUCKET:
    return valueTransferStmt();
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