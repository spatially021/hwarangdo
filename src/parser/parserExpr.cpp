#include "../include/Parser.h"
#include <memory>

using Ptr = Expr::Ptr;

Ptr Parser::assignment() {
  Token t = peek();
  Ptr left = ternary();
  if (isAssign()) {
    Token op = advance();
    Ptr right = expression();
    if (!isAssginable(left))
      error(left->token, "Invalid assignment target");
    return make_shared<AssignExpr>(t, left, op, right);
  }
  return left;
}

Ptr Parser::ternary() {
  Ptr left = logicalOr();
  if (check(TKind::QUESTION)) {
    advance(); //?처리
    Ptr then = assignment();
    consume(TKind::COLON, "expect ':' after condition");
    Ptr else_ = assignment();
    return make_shared<TernaryExpr>(left->token, left, then, else_);
  }
  return left;
}

Ptr Parser::logicalOr() {
  Ptr left = logicalAnd();
  while (check(TKind::OR)) {
    Token op = advance();
    Ptr right = logicalAnd();
    left = make_shared<BinaryExpr>(left->token, left, op, right);
  }
  return left;
}

Ptr Parser::logicalAnd() {
  Ptr left = equality();
  while (check(TKind::AND)) {
    Token op = advance();
    Ptr right = equality();
    left = make_shared<BinaryExpr>(left->token, left, op, right);
  }
  return left;
}

Ptr Parser::equality() {
  Ptr left = comparison();
  if (check({TKind::DOUBLE_EQUAL, TKind::BANG_EQUAL})) {
    Token op = advance();
    Ptr right = comparison();
    return make_shared<BinaryExpr>(left->token, left, op, right);
  }
  return left;
}

Ptr Parser::comparison() {
  Ptr left = term();
  if (check({TKind::GREATER, TKind::GREATER_EQUAL, TKind::LESS,
             TKind::LESS_EQUAL})) {
    Token op = advance();
    Ptr right = term();
    return make_shared<BinaryExpr>(left->token, left, op, right);
  }
  return left;
}

Ptr Parser::term() {
  Ptr left = factor();
  while (check({TKind::PLUS, TKind::MINUS})) {
    Token op = advance();
    Ptr right = factor();
    left = make_shared<BinaryExpr>(left->token, left, op, right);
  }
  return left;
}

Ptr Parser::factor() {
  Ptr left = power();
  while (check({TKind::STAR, TKind::SLASH, TKind::PERCENT})) {
    Token op = advance();
    Ptr right = power();
    left = make_shared<BinaryExpr>(left->token, left, op, right);
  }
  return left;
}

Ptr Parser::power() {
  Ptr left = unary();
  if (check(TKind::DOUBLE_STAR)) {
    Token op = advance();
    Ptr right = power();
    return make_shared<BinaryExpr>(left->token, left, op, right);
  }
  return left;
}

Ptr Parser::unary() {
  if (check({TKind::MINUS, TKind::PLUS, TKind::BANG})) {
    Ptr left = postfix();
    Token op = advance();
    return make_shared<UnaryExpr>(left->token, op, left);
  }
  return postfix();
}

Ptr Parser::postfix() {
  Expr::Ptr expr = primary();
  while (true) {
    if (check(TKind::DOT)) {
      Token t = advance(); //.처리
      Token member =
          consume(TKind::IDENTIFIER, "expect member's name after '.'");
      expr = make_shared<MemberExpr>(t, expr, member.text);
    } else if (check(TKind::LEFT_PAREN)) {
      Token t = advance(); //(처리
      std::vector<Expr::Ptr> args;
      while (!check(TKind::RIGHT_PAREN) && !isAtEnd()) {
        args.push_back(ternary());
        if (check(TKind::COMMA) && !check(TKind::RIGHT_PAREN, 1)) {
          advance(); //,처리
        }
      }
      consume(TKind::RIGHT_PAREN, "expect ')' after arguments");
      expr = make_shared<CallExpr>(t, expr, args);
    } else if (check(TKind::LEFT_BRACKET)) {
      Token t = advance(); //[처리
      Expr::Ptr index = ternary();
      consume(TKind::RIGHT_BRACKET, "expect ']' after index");
      expr = make_shared<IndexExpr>(t, expr, index);
    } else
      break;
  }
  return expr;
}

Ptr Parser::primary() {
  Token t = peek();

  if (check({TKind::LIT_INT, TKind::LIT_BOOL, TKind::LIT_FLOAT,
             TKind::LIT_CHARACTER, TKind::LIT_STRING}))
    return make_shared<LiteralExpr>(t, advance().text);
  if (check(TKind::IDENTIFIER))
    return make_shared<VarExpr>(t, advance().text);
  if (check(TKind::LEFT_PAREN)) {
    advance();
    Expr::Ptr expr = expression();
    consume(TKind::RIGHT_PAREN, "expect ')' after expression");
    return expr;
  }
  if (check(TKind::SUPER))
    return make_shared<SuperExpr>(advance());

  if (check(TKind::THIS))
    return make_shared<ThisExpr>(advance());



  error(peek(), "expect expression");
}

