#pragma once
#include "Token.h"
#include <string>
#include <vector>

using namespace std;

struct Lexer {
  explicit Lexer(const string &src);
  Token next();
  vector<Token> tokenized;

public:
  Token scan();
  void lexing();

protected:
  char peek(int offset = 0) const;
  char get();
  void skipWS();
  bool isIdentFirst(char c);
  bool isIdentRest(char c);
  bool isNumber(char c);
  string src;
  size_t i = 0;
  int line, col;
  unsigned int pos;
  bool isEscapeChar(char c);

  std::unordered_map<std::string, TKind> keyword_map = {
      {"int", TKind::INT},           {"float", TKind::FLOAT},
      {"fixed", TKind::FIXED},       {"char", TKind::CHAR},
      {"string", TKind::STRING},     {"bool", TKind::BOOL},
      {"null", TKind::NUL},          {"if", TKind::IF},
      {"else", TKind::ELSE},         {"switch", TKind::SWITCH},
      {"case", TKind::CASE},         {"for", TKind::FOR},
      {"while", TKind::WHILE},       {"break", TKind::BREAK},
      {"continue", TKind::CONTINUE}, {"return", TKind::RETURN},
      {"func", TKind::FUNC},         {"void", TKind::VOID},
      {"borrow", TKind::BORROW},     {"mut", TKind::MUT},
      {"share", TKind::SHARE},       {"weak", TKind::WEAK},
      {"class", TKind::CLASS},       {"struct", TKind::STRUCT},
      {"public", TKind::PUBLIC},     {"protected", TKind::PROTECTED},
      {"private", TKind::PRIVATE},   {"internal", TKind::INTERNAL},
      {"impl", TKind::IMPL},         {"trait", TKind::TRAIT},
      {"extends", TKind::EXTENDS},{"enum",TKind::ENUM},
      {"try",TKind::TRY},{"catch",TKind::CATCH},
      {"new",TKind::NEW},{"root",TKind::ROOT},
      {"const",TKind::CONST}
      };
};
