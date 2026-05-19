#include "AST/Decl.h"
#include "AST/Expr.h"
#include "AST/Stmt.h"
#include "Parser.h"
#include "SourceSpan.h"
#include "Token.h"
#include "util/Error.h"
#include <memory>
#include <optional>

using Ptr = Stmt::Ptr;
using namespace std;

Ptr Parser::expressionStmt() {
  Token t = peek();
  Expr::Ptr expr = expression();
  consume(TKind::SEMICOLON, "expect ';' after expression statement");
  return make_shared<ExprStmt>(makeSpan(t.span, expr->span), expr);
}

Ptr Parser::declStmt() {
  Token t = peek();
  Decl::Ptr decl = declaration(contexts.back());
  return make_shared<DeclStmt>(makeSpan(t.span, decl->span), decl);
}

Ptr Parser::ifStmt() {
  Token t = peek();
  advance(); // if처리
  consume(TKind::LEFT_PAREN, "expect '(' after if");
  Expr::Ptr condition = expression();
  consume(TKind::RIGHT_PAREN, "expect ')' after condition");

  Ptr thenBranch = bodyStmt();
  Ptr elseBranch = nullptr;
  if (check(TKind::ELSE)) {
    advance(); // else 처리
    elseBranch = bodyStmt();
  }

  return make_shared<IfStmt>(
      makeSpan(t.span, elseBranch ? elseBranch->span : thenBranch->span),
      condition, thenBranch, elseBranch);
}

Ptr Parser::blockStmt() {
  Token t = peek();
  vector<Ptr> s;
  while (!check(TKind::RIGHT_BRACE) && !isAtEnd()) {
    s.push_back(statement());
  }

  consume(TKind::RIGHT_BRACE, "expect '}' end of block");
  auto end = previous();
  return make_shared<BlockStmt>(makeSpan(t, end), s);
}

Ptr Parser::bodyStmt() {
  ContextGuard _{contexts, DeclContext::BLOCK};
  if (check(TKind::LEFT_BRACE)) {
    advance(); //{처리
    return blockStmt();
  }
  return statement();
}

Ptr Parser::forStmt() {
  Token t = peek();
  advance(); // for 처리
  consume(TKind::LEFT_PAREN, "exepct '(' after for");

  Ptr init = declStmt();
  auto decl = dynamic_cast<DeclStmt *>(init.get());
  if (decl == nullptr) {
    Error::internal(t, "for's var init is not declStmt");
  }
  auto var = dynamic_cast<VarDecl *>(decl->decl.get());
  if (var == nullptr) {
    Error::internal(t, "for's var inti is not varDecl");
  }
  if (var->init != nullptr)
    Error::diagnostic(t, "in for statement initiate expression not available");
  Expr::Ptr from = expression();
  consume(TKind::DOUBLE_DOT, "range need '..'");
  Expr::Ptr to = expression();
  Expr::Ptr step = nullptr;
  if (check(TKind::BY)) {
    advance(); // advance by
    step = expression();
  }
  consume(TKind::RIGHT_PAREN, "expect ')'");
  shared_ptr<Range> range =
      make_shared<Range>(makeSpan(from->span, to->span), from, to, step);
  Ptr body = bodyStmt();
  return make_shared<ForStmt>(makeSpan(t.span, body->span), init, range, body);
}

Ptr Parser::whileStmt() {
  Token t = peek();
  advance(); // while처리
  consume(TKind::LEFT_PAREN, "expect '(' after while");
  Expr::Ptr conditon = expression();
  consume(TKind::RIGHT_PAREN, "expect ')' after condition");
  Ptr body = bodyStmt();
  return make_shared<WhileStmt>(makeSpan(t.span, body->span), conditon, body);
}

Ptr Parser::switchStmt() {
  Token t = peek();
  advance(); // switch처리

  consume(TKind::LEFT_PAREN, "expect '(' after swtich");
  Expr::Ptr value = expression();
  consume(TKind::RIGHT_PAREN, "expect ')'");
  consume(TKind::LEFT_BRACE, "expect '{' after '(' in switch statement");
  vector<shared_ptr<Case>> cases;
  while (!check(TKind::RIGHT_BRACE) && !isAtEnd()) {
    cases.push_back(caseStmt());
  }
  auto end = previous();
  return make_shared<SwitchStmt>(makeSpan(t, end), value, cases);
}

shared_ptr<Case> Parser::caseStmt(bool isSwtich) {
  Token tok = peek();
  if (check(TKind::CASE)) {
    advance(); // case 처리
    vector<Expr::Ptr> values;
    while (!check(TKind::EQAUL_AGNLEBUCKET) && !isAtEnd()) {
      auto temp = dynamic_pointer_cast<CaseValueExpr>(parseCaseValue());
      if (!temp) {
        Error::internal("illegal expr kind");
      }
      values.push_back(temp);
      if (check(TKind::COMMA)) {
        if (check(TKind::EQAUL_AGNLEBUCKET, 1))
          Error::diagnostic(following(), "expect expression");
        else
          advance(); //,처리
      }
    }
    consume(TKind::EQAUL_AGNLEBUCKET, "expect '=>' after condition(s)");
    Ptr body;
    if (check(TKind::LEFT_BRACE)) {
      body = bodyStmt();
    } else
      Error::diagnostic(peek(), "expect '{' after '=>'");

    auto end = previous();
    return make_shared<Case>(makeSpan(tok, end), values, body);
  } else if (check(TKind::DEFAULT)) {
    if (!isSwtich) {
      Error::diagnostic(tok, "in match not allowed 'default'");
    }
    advance(); // default 처리
    consume(TKind::EQAUL_AGNLEBUCKET, "expect '=>' after default");
    vector<Expr::Ptr> values;
    Ptr body;
    if (check(TKind::LEFT_BRACE))
      body = bodyStmt();
    else
      Error::diagnostic(peek(), "expect '{' after '=>'");
    return make_shared<Case>(makeSpan(tok.span, body->span), values, body,
                             true);
  } else if (!check(TKind::UNDERBAR)) {
    if (isSwtich) {
      Error::diagnostic(tok, "in switch not allowed '_'");
    }
    advance(); // _ 처리
    consume(TKind::EQAUL_AGNLEBUCKET, "expect '=>' after _");
    vector<Expr::Ptr> values;
    Stmt::Ptr body;
    if (check(TKind::LEFT_BRACE))
      body = bodyStmt();
    else
      Error::diagnostic(peek(), "expect '{' after '=>'");
    return make_shared<Case>(makeSpan(tok.span, body->span), values, body,
                             true);
  } else {
    if (isSwtich) {
      Error::diagnostic(peek(),
                        "in switch statement place only case or default");
    } else {
      Error::diagnostic(peek(), "in match expression place only case or _");
    }
  }
}

Ptr Parser::returnStmt() {
  Token t = peek();
  advance(); // return 처리;
  Expr::Ptr expr;
  if (check(TKind::SEMICOLON))
    expr = nullptr;
  else
    expr = expression();
  consume(TKind::SEMICOLON, "expect ';' after return statement");
  auto end = previous();
  return make_shared<ReturnStmt>(makeSpan(t, end), expr);
}

Ptr Parser::valueTransferStmt() {
  Token t = peek();
  advance(); //<<처리
  Expr::Ptr expr = expression();
  consume(TKind::SEMICOLON, "expect ';' after valueTransferOperator statement");
  auto end = previous();
  return make_shared<ValueTransferStmt>(makeSpan(t, end), expr);
}

Ptr Parser::tryStmt() {
  Token t = peek();
  advance(); // try 처리
  Ptr body = bodyStmt();
  vector<shared_ptr<CatchClause>> catches;
  while (check(TKind::CATCH))
    catches.push_back(dynamic_pointer_cast<CatchClause>(catchStmt()));
  auto end = previous();
  return make_shared<TryCatchStmt>(makeSpan(t, end), body, catches);
}

Ptr Parser::catchStmt() {
  Token t = peek();
  advance(); // catch 처리
  consume(TKind::LEFT_BRACE, "expect '(' after catch");
  Token errorType = consume(TKind::IDENTIFIER, "expect error type after '('");
  optional<string> name = nullopt;
  if (!check(TKind::RIGHT_BRACE))
    name =
        consume(TKind::IDENTIFIER, "expect error identifier after error type")
            .text;

  consume(TKind::RIGHT_BRACE, "expect ')' end of catch()");
  Ptr body = bodyStmt();

  TypeNode::Ptr type = typeNodeConvertor(errorType);
  auto end = previous();
  return make_shared<CatchClause>(makeSpan(t, end), type, name, body);
}

Ptr Parser::onexitStmt() {
  Token t = peek();
  advance(); // onexit 처리
  consume(TKind::LEFT_BRACE, "expect '{' after onexit");
  Ptr body = blockStmt();
  return make_shared<OnexitStmt>(makeSpan(t.span, body->span), body);
}

Ptr Parser::throwStmt() {
  Token t = peek();
  advance(); // throw 처리
  Expr::Ptr expr = expression();
  return make_shared<ThrowStmt>(makeSpan(t.span, expr->span), expr);
}