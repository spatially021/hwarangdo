#pragma once

#include "./Recover.h"

struct Lexer;

class LexerRecover final : public Recover {
public:
  LexerRecover(Lexer &lexer);
  void recover();

private:
  Lexer &lexer;
};