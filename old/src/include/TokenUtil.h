#pragma once
#include "Token.h"
#include <string>

inline std::string tokKindToString(TokKind kind) {
  switch (kind) {
  case TokKind::LEFT_PAREN:
    return "LEFT_PAREN";
  case TokKind::RIGHT_PAREN:
    return "RIGTH_PAREN";
  case TokKind::LEFT_BRACE:
    return "LEFT_BRACE";
  case TokKind::RIGHT_BRACE:
    return "RIGTH_BRACE";
  case TokKind::LEFT_BRACKET:
    return "LEFT_BRACKET";
  case TokKind::RIGHT_BRACKET:
    return "RIGTH_BTACKET";

  case TokKind::SEMICOLON:
    return "SEMICOLON";
  case TokKind::COLON:
    return "COLON";
  case TokKind::COMMA:
    return "COMMA";
  case TokKind::DOT:
    return "DOT";
  case TokKind::PLUS:
    return "PLUS";
  case TokKind::DOUBLE_PLUS:
    return "DOUBLE_PLUS";
  case TokKind::MINUS:
    return "MINUS";
  case TokKind::DOUBLE_MINUS:
    return "DOUBLE_MINS";
  case TokKind::STAR:
    return "STAR";
  case TokKind::DOUBLE_STAR:
    return "DOUBLE_STAR";
  case TokKind::STAR_SLASH:
    return "STAR_SLASH";
  case TokKind::SLASH:
    return "SLASH";
  case TokKind::DOUBLE_SLASH:
    return "DOUBLE_SLASH";
  case TokKind::SLASH_STAR:
    return "SLASH_STAR";
  case TokKind::PERCENT:
    return "PERCENT";

  case TokKind::EQUAL:
    return "EQUAL";
  case TokKind::EQUAL_EQUAL:
    return "EQUAL_EQUAL";
  case TokKind::EXCLAIM:
    return "EXCLAIM";
  case TokKind::EXCLAIM_EQUAL:
    return "EXCLAIM_EQUAL";
  case TokKind::GREATER:
    return "GREATER";
  case TokKind::GREATER_EQUAL:
    return "GREATER_EQUAL";
  case TokKind::LESS:
    return "LESS";
  case TokKind::LESS_EQUAL:
    return "LESS_EQUAL";

  case TokKind::IDENTIFIER:
    return "IDENTIFIER";

  case TokKind::BOOLEAN:
    return "BOOLEAN";
  case TokKind::INTEGER:
    return "INTEGER";
  case TokKind::FLOAT:
    return "FLOAT";
  case TokKind::DOUBLE:
    return "DOUBLE";
  case TokKind::CHAR:
    return "CHAR";
  case TokKind::STRING:
    return "STRING";
  case TokKind::KW_NULL:
    return "KW_NULL";

  case TokKind::END:
    return "END";

  case TokKind::IF:
    return "IF";
  case TokKind::ELSE:
    return "ELSE";
  case TokKind::AND:
    return "AND";
  case TokKind::OR:
    return "OR";
  case TokKind::KW_INT:
    return "KW_INT";
  case TokKind::KW_FLOAT:
    return "KW_FLOAT";
  case TokKind::KW_DOUBLE:
    return "KW_DOUBLE";
  case TokKind::KW_BOOLEAN:
    return "KW_BOOLEAN";
  case TokKind::KW_CHAR:
    return "KW_CHAR";
  case TokKind::KW_STRING:
    return "KW_STRING";
  case TokKind::FOR:
    return "FOR";
  case TokKind::WHILE:
    return "WHILE";
  case TokKind::RETURN:
    return "RETURN";
  case TokKind::SWITCH:
    return "SWITCH";
  case TokKind::CASE:
    return "CASE";

  case TokKind::F_INT:
    return "F_INT";
  case TokKind::F_FlOAT:
    return "F_FlOAT";
  case TokKind::F_DOUBLE:
    return "F_DOUBLE";
  case TokKind::F_BOOLEAN:
    return "F_BOOLEAN";
  case TokKind::F_CHAR:
    return "F_CHAR";
  case TokKind::F_STRING:
    return "F_STRING";
  case TokKind::F_VOID:
    return "F_VOID";
  case TokKind::FUNC:
    return "FUNC";

  case TokKind::CLASS:
    return "CLASS";

  default:
    return "UNKNOWN";
  }
}
