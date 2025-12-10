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
  void lexe();

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
};
