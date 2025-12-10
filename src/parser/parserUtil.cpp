#pragma once

#include "../include/Parser.h"
#include <regex>
#include <stdexcept>

bool Parser::isAtEnd() const {
  return current >= tokens.size() || peek().kind == TKind::END;
}
const Token &Parser::consume(TKind kind, const string &message) {
  if (check(kind))
    return advance();
  error(peek(), message);
  throw runtime_error("");
}

[[noreturn]]
void Parser::error(const Token &token, const string &message) const {
  string m = "[line ";
  m += std::to_string(token.line);
  m += "] Error at '" + token.text + "': " + message;

  throw runtime_error(m);
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
    if (check(kind,step)) {
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
    return tokens[current+step];
  }
  throw runtime_error("No following token");
}
bool Parser::isValidSize(const std::string &s) const {
  static const std::regex signedPattern(R"(^\d+$)");
  static const std::regex unsignedPattern(R"(^[uU]\d+$)");
  static const std::regex fixedPattern(R"(^\d+\.\d+$)");
  static const std::regex unsignedFixedPattern(R"(^[uU]\d+\.\d+$)");

  return std::regex_match(s, signedPattern) ||
         std::regex_match(s, unsignedPattern) ||
         std::regex_match(s, fixedPattern)||
         std::regex_match(s,unsignedFixedPattern);
}

inline bool Parser::isAccessModifier() const {
  auto k = peek().kind;
  return k == TKind::PUBLIC || k == TKind::PRIVATE || k == TKind::PROTECTED;
}

AModifier Parser::AModifierConvertor(Token t){
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
    error(following(), "Expected type after access modifier");
  }
  return isTypeToken(peek().kind);
}
