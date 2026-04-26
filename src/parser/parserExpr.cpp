#include "AST/ASTNode.h"
#include "AST/Expr.h"
#include "Parser.h"
#include "Token.h"
#include "util/Error.h"
#include <memory>
#include <string>

using Ptr = Expr::Ptr;

Ptr Parser::assignment() { // 대입 연산 처리
  Token t = peek();
  Ptr left = ternary();
  if (isAssign()) {
    Token op = advance();
    Ptr right = expression();
    if (!isAssginable(left))
      Error::diagnostic(left->token, "Invalid assignment target");
    return make_shared<AssignExpr>(t, left, op, right);
  }
  return left;
}

Ptr Parser::ternary() { // 삼항 연산 처리
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
  // Ptr left = equality();//
  Ptr left = bitOr();
  while (check(TKind::AND)) {
    Token op = advance();
    // Ptr right = equality();
    Ptr right = bitOr();
    left = make_shared<BinaryExpr>(left->token, left, op, right);
  }
  return left;
}

Ptr Parser::bitOr() {
  Ptr left = bitXor();
  while (check(TKind::PIPE)) {
    Token op = advance();
    Ptr right = bitXor();
    left = make_shared<BinaryExpr>(left->token, left, op, right);
  }
  return left;
}

Ptr Parser::bitXor() {
  Ptr left = bitAnd();
  while (check(TKind::CARET)) {
    Token op = advance();
    Ptr right = bitAnd();
    left = make_shared<BinaryExpr>(left->token, left, op, right);
  }
  return left;
}

Ptr Parser::bitAnd() {
  Ptr left = equality();
  while (check(TKind::AMPERSAND)) {
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
    Token op = advance();
    Ptr left = postfix();
    return make_shared<UnaryExpr>(left->token, op, left);
  }
  return postfix();
}

Ptr Parser::postfix() {
  Expr::Ptr expr = primary();

  while (true) {
    if (check(TKind::DOT)) {
      Token t = advance(); //.처리

      if (expr->kind == NKind::BUILTIN_NAME_EXPR) { // built in method 처리
        if (check(TKind::IDENTIFIER) && peek().text == "spawn" &&
            expr->kind == NKind::BUILTIN_NAME_EXPR) {
          advance(); // spawn 처리
          TypeNode::Ptr type = parseType();
          consume(TKind::LEFT_PAREN, "expect (");
          std::vector<Expr::Ptr> args;
          if (!check(TKind::RIGHT_PAREN)) {
            do {
              args.push_back(ternary());
            } while (match({TKind::COMMA}));
          }

          consume(TKind::RIGHT_PAREN, "expect ')' after arguments");
          return make_shared<SpawnExpr>(t, expr, type, args);
        } else if (check(TKind::IDENTIFIER) && peek().text == "view") {
          advance(); // view 처리
          consume(TKind::LEFT_PAREN, "expect '(' after view");
          Ptr target = expression();
          consume(TKind::RIGHT_PAREN, "expect ')' after arguments");
          return make_shared<ViewExpr>(t, expr, target);
        } else if (check(TKind::IDENTIFIER) && peek().text == "destroy") {
          advance(); // destroy 처리
          consume(TKind::LEFT_PAREN, "expect '(' after destroy");
          Ptr target = expression();
          consume(TKind::RIGHT_PAREN, "expect ')' after arguments");
          return make_shared<DestroyExpr>(t, expr, target);
        }

        else {
          Error::diagnostic(peek(),
                            "unknown storage's method : " + peek().text);
        }
      }

      Token member =
          consume(TKind::IDENTIFIER, "expect member's name after '.'");
      expr = make_shared<MemberExpr>(t, expr, member.text);
    } else if (check(TKind::LEFT_PAREN)) {
      Token t = advance(); //(처리
      std::vector<Expr::Ptr> args;
      if (!check(TKind::RIGHT_PAREN)) {
        do {
          args.push_back(ternary());
        } while (match({TKind::COMMA}));
      }

      consume(TKind::RIGHT_PAREN, "expect ')' after arguments");

      if (expr->kind == NKind::MEMBER_EXPR) {
        auto member = static_pointer_cast<MemberExpr>(expr);
        expr = make_shared<CallExpr>(t, member->object, member->member, args);
      } else if (expr->kind == NKind::NAME_EXPR) {
        expr = make_shared<CallExpr>(
            t, nullptr, static_pointer_cast<NameExpr>(expr)->name, args);
      } else
        Error::diagnostic(t, "expression is not callable");

    } else if (check(TKind::LEFT_BRACKET)) {
      Token t = advance(); //[처리
      Expr::Ptr index = ternary();
      consume(TKind::RIGHT_BRACKET, "expect ']' after index");
      expr = make_shared<ArrayAccessExpr>(t, expr, index);
    } else if (check(TKind::CAST)) {
      Token t = advance(); // as처리
      TypeNode::Ptr type = parseType();
      expr = make_shared<CastExpr>(t, expr, type);
    } else
      break;
  }

  return expr;
}

Ptr Parser::primary() {
  Token t = peek();

  if (isLit())
    return make_shared<LiteralExpr>(t, advance().text);
  if (check(TKind::IDENTIFIER))
    return make_shared<NameExpr>(t, advance().text);
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
  if (check(TKind::SELF)) {
    return make_shared<SelfExpr>(advance());
  }
  if (check(TKind::ROOT)) {
    return make_shared<RootExpr>(advance());
  }

  if (check({TKind::WORLD, TKind::ARENA})) {
    return make_shared<BuiltInNameExpr>(t, advance().text);
  }

  if (check(TKind::UNDERBAR)) {
    return make_shared<DefaultValueExpr>(advance());
  }

  if (check(TKind::MATCH)) {
    advance(); // match처리
    consume(TKind::LEFT_PAREN, "expect '(' after match");
    Ptr value = expression();
    consume(TKind::RIGHT_PAREN, "expect ')' after valye");
    consume(TKind::LEFT_BRACE, "expect '{' after '('");
    vector<shared_ptr<Case>> cases;
    while (!check(TKind::RIGHT_BRACE) && !isAtEnd()) {
      auto c = caseStmt(false);
      if (!c->isDefault && c->values.size() != 1) {
        Error::diagnostic(c->token, "in match case can have one value but " +
                                        to_string(c->values.size()));
      }
      cases.push_back(c);
    }
    consume(TKind::RIGHT_BRACE, "expect '}' after match body");
    return make_shared<MatchExpr>(t, value, cases);
  }

  Error::diagnostic(peek(), "expect expression : " + peek().text);
}
