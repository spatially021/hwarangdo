#include "AST/ASTNode.h"
#include "AST/Expr.h"
#include "Parser.h"
#include "Token.h"
#include "util/Error.h"
#include <iterator>
#include <memory>
#include <stdexcept>

bool Parser::isAtEnd() const {
  return current >= tokens.size() || peek().kind == TKind::END;
}
const Token &Parser::consume(TKind kind, const string &message) {
  if (check(kind))
    return advance();
  Error::diagnostic(peek(), message);
  throw runtime_error("");
}

bool Parser::match(std::initializer_list<TKind> kinds) {
  for (auto kind : kinds) {
    if (check(kind)) {
      advance();
      return true;
    }
  }
  return false;
}

bool Parser::check(std::initializer_list<TKind> kinds, size_t step) const {
  for (auto kind : kinds) {
    if (check(kind, step)) {
      return true;
    }
  }
  return false;
}

bool Parser::check(TKind kind, size_t step) const {
  if (isAtEnd())
    return false;
  return following(step).kind == kind;
}

const Token &Parser::advance() {
  if (!isAtEnd())
    current++;
  return previous();
}

const Token &Parser::peek() const {
  if (current >= tokens.size()) {
    throw runtime_error(("Peek out of range"));
  }

  return tokens[current];
}

const Token &Parser::previous() const {
  if (current == 0)
    throw runtime_error("No previous token");
  return tokens[current - 1];
}

const Token &Parser::following(size_t step) const {
  if (current + step < tokens.size()) {
    return tokens[current + step];
  }
  throw runtime_error("No following token");
}

bool Parser::isAccessModifier() const {
  auto k = peek().kind;
  return k == TKind::PUBLIC || k == TKind::PRIVATE || k == TKind::PROTECTED;
}

AModifier Parser::AModifierConvertor(Token t) {
  AModifier modi = AModifier::DEFAULT;
  if (t.kind == TKind::PUBLIC)
    modi = AModifier::PUBLIC;
  else if (t.kind == TKind::PRIVATE)
    modi = AModifier::PRIVATE;
  else if (t.kind == TKind::PROTECTED)
    modi = AModifier::PROTECTED;
  return modi;
}

bool Parser::isTypeToken(TKind k) const {
  switch (k) {
  case TKind::INT:
  case TKind::FLOAT:
  case TKind::CHAR:
  case TKind::STRING:
  case TKind::FIXED:
  case TKind::BOOL:
  case TKind::IDENTIFIER:
    return true;
  default:
    return false;
  }
}

bool Parser::isType() const {
  if (isAccessModifier()) {
    if (isTypeToken(following().kind))
      return true;
    Error::diagnostic(following(), "Expected type after access modifier");
  }
  return isTypeToken(peek().kind);
}

bool Parser::isFunc() const {
  if (isAccessModifier()) {
    if (isTypeToken(following().kind) || following().kind == TKind::VOID ||
        following().kind == TKind::FUNC) {
      return following(2).kind == TKind::IDENTIFIER &&
             following(3).kind == TKind::LEFT_PAREN;
    }
    Error::diagnostic(following(), "expect type after access modifier");
  }

  if (isTypeToken(peek().kind) || peek().kind == TKind::VOID ||
      peek().kind == TKind::FUNC) {
    return following().kind == TKind::IDENTIFIER &&
           following(2).kind == TKind::LEFT_PAREN;
  }

  return false;
}

TypeNode::Ptr Parser::typeNodeConvertor(Token ty, Token size) {
  TypeNode::Ptr node;
  if (ty.kind == TKind::IDENTIFIER) {
    node = make_shared<IdentifierTypeNode>(ty, ty.text);
  } else {
    switch (ty.kind) {
    case TKind::INT:
      node = make_shared<BuiltinTypeNode>(ty, BuiltinTypeNode::Category::Int,
                                          size);
      break;
    case TKind::FLOAT:
      node = make_shared<BuiltinTypeNode>(ty, BuiltinTypeNode::Category::Float,
                                          size);
      break;
    case TKind::FIXED:
      node = make_shared<BuiltinTypeNode>(ty, BuiltinTypeNode::Category::Fixed,
                                          size);
      break;
    case TKind::BOOL:
      node = make_shared<BuiltinTypeNode>(ty, BuiltinTypeNode::Category::Bool,
                                          size);
      break;
    case TKind::CHAR:
      node = make_shared<BuiltinTypeNode>(ty, BuiltinTypeNode::Category::CHAR,
                                          size);
      break;
    case TKind::STRING:
      node = make_shared<BuiltinTypeNode>(ty, BuiltinTypeNode::Category::STRING,
                                          size);
      break;
    case TKind::VOID:
      node = make_shared<BuiltinTypeNode>(ty, BuiltinTypeNode::Category::Void);
      break;
    case TKind::FUNC:
      node = make_shared<BuiltinTypeNode>(ty, BuiltinTypeNode::Category::FUNC);
      break;

    default:
      Error::diagnostic(ty, "unexpected type : " + ty.text);
    }
  }
  return node;
}

bool Parser::isAssign() const {
  return check({TKind::EQUAL, TKind::PLUS_EQUAL, TKind::MINUS_EQUAL,
                TKind::STAR_EQUAL, TKind::DOUBLE_STAR_EQUAL, TKind::SLASH_EQUAL,
                TKind::PERCENT_EQUAL, TKind::AMPERSAND_EQAUL, TKind::PIPE_EQUAL,
                TKind::CARET_EQUAL, TKind::DOUBLE_ANGLEBUCKET_EQAUL,
                TKind::DOUBLE_RIGHT_ANGLE_BUCKET_EQUAL});
}

bool Parser::isAssginable(Expr::Ptr p) const {
  switch (p->kind) {
  case NKind::NAME_EXPR:
  case NKind::MEMBER_EXPR:
  case NKind::ARRAY_ACCESS_EXPR:
    return true;
  default:
    return false;
  }
}

TypeNode::Ptr Parser::parseType() {
  Token ty = advance(); // 자료형/객체명
  Token size = {};

  if (check(TKind::COLON)) {
    if (ty.kind == TKind::IDENTIFIER)
      Error::diagnostic(peek(), "':' is only allowed built-in types");

    advance(); // :
    size = consume(TKind::SIZE, "expect size value");
    assert(size.text != "");

    switch (ty.kind) {
    case TKind::INT:
      if (!(size.text[0] == 'i' || size.text[0] == 'u'))
        Error::diagnostic(size, "unmatch bitwidth type");
      break;
    case TKind::FLOAT:
      if (size.text[0] != 'f')
        Error::diagnostic(size, "unmatch bitwidth type");
      break;
    case TKind::FIXED:
      break;
    case TKind::CHAR:
      if (size.text[0] != 'c')
        Error::diagnostic(size, "unmatch bitwidth type");
      break;
    case TKind::STRING:
      if (size.text[0] != 's')
        Error::diagnostic(size, "unmatch bitwidth type");
      break;
    default:
      Error::diagnostic(ty, "unexpected type : " + ty.text);
    }
  }
  std::vector<std::pair<Token, Expr::Ptr>> dims;
  while (check(TKind::LEFT_BRACKET)) {
    Token bracket = advance();
    Expr::Ptr sizeExpr = expression();
    consume(TKind::RIGHT_BRACKET, "expect ']'");
    dims.push_back({bracket, std::move(sizeExpr)});
  }

  TypeNode::Ptr type = typeNodeConvertor(ty, size);

  for (auto it = dims.rbegin(); it != dims.rend(); ++it) {
    type = make_shared<ArrayTypeNode>(it->first, type, std::move(it->second));
  }

  return type;
}

void Parser::notFunc(DeclPrefix prefix) {

  if (prefix.isFrame) {
    Error::diagnostic(previous(), "frame can place only function declaration");
  }
  if (prefix.isOverride) {
    Error::diagnostic(previous(),
                      "override can place only function declaration");
  }

  if (prefix.isAsync) {
    Error::diagnostic(previous(), "async can place only function declaration");
  }
}

void Parser::notVar(DeclPrefix prefix) {
  if (prefix.isConst)
    Error::diagnostic(previous(), "const can place only variation declaration");
  if (prefix.isRoot) {
    Error::diagnostic(previous(), "root can place only variation declaration");
  }
}

Expr::Ptr Parser::parseCaseValue() {
  Token t = peek();
  Expr::Ptr args = nullptr;
  if (isLit()) {
    auto ad = advance();
    return make_shared<CaseValueExpr>(
        makeSpan(t, ad), make_shared<LiteralExpr>(ad.span, ad, ad.text),
        nullptr);
  }
  if (check(TKind::IDENTIFIER)) {
    auto pay = advance();
    Expr::Ptr expr = make_shared<NameExpr>(makeSpan(t, pay), pay.text);
    if (match({TKind::DOT})) {
      Token member = consume(TKind::IDENTIFIER, "expect ident after '.'");
      expr = make_shared<MemberExpr>(makeSpan(t, member), expr, member.text);
    }
    if (check(TKind::LEFT_PAREN)) {
      advance(); //(처리
      auto id = consume(TKind::IDENTIFIER, "after '(' expect id");
      consume(TKind::RIGHT_PAREN, "after id expect ')'");
      args = make_shared<NameExpr>(makeSpan(t, id), id.text);
    }
    auto end = previous();
    return make_shared<CaseValueExpr>(makeSpan(t, end), expr, args);
  }
  Error::diagnostic(t, "in case value only allow literal or Enum : " + t.text);
}