#pragma once
#include "AST/TokenStream.h"
#include "Inputs.h"
#include "Token.h"
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

struct Lexer {
  explicit Lexer(InputSource inputs);
  Token next();
  vector<Token> tokenized;

public:
  Token scan();
  TokenStream lexing();

protected:
  char peek(unsigned int offset = 0) const;
  char get();
  void skipWS();
  bool isIdentFirst(char c);
  bool isIdentRest(char c);
  bool isNumber(char c);
  InputSource input;
  size_t i = 0;
  int line, col;
  string src;
  string path;
  unsigned int pos;
  bool isEscapeChar(char c);

  std::unordered_map<std::string, TKind> keyword_map = {
      {"int", TKind::INT},
      {"float", TKind::FLOAT},
      {"fixed", TKind::FIXED},
      {"char", TKind::CHAR},
      {"string", TKind::STRING},
      {"bool", TKind::BOOL},
      {"null", TKind::NUL},
      {"if", TKind::IF},
      {"else", TKind::ELSE},
      {"switch", TKind::SWITCH},
      {"case", TKind::CASE},
      {"for", TKind::FOR},
      {"while", TKind::WHILE},
      {"break", TKind::BREAK},
      {"continue", TKind::CONTINUE},
      {"return", TKind::RETURN},
      {"func", TKind::FUNC},
      {"void", TKind::VOID},
      {"class", TKind::CLASS},
      {"struct", TKind::STRUCT},
      {"public", TKind::PUBLIC},
      {"protected", TKind::PROTECTED},
      {"private", TKind::PRIVATE},
      {"internal", TKind::INTERNAL},
      {"impl", TKind::IMPL},
      {"trait", TKind::TRAIT},
      {"extends", TKind::EXTENDS},
      {"enum", TKind::ENUM},
      {"try", TKind::TRY},
      {"catch", TKind::CATCH},
      {"new", TKind::NEW},
      {"root", TKind::ROOT},
      {"const", TKind::CONST},
      {"", TKind::EMPTY},
      {"default", TKind::DEFAULT},
      {"match", TKind::MATCH},
      {"throw", TKind::THROW},
      {"super", TKind::SUPER},
      {"this", TKind::THIS},
      {"i8", TKind::SIZE},
      {"i16", TKind::SIZE},
      {"i32", TKind::SIZE},
      {"i64", TKind::SIZE},
      {"i128", TKind::SIZE},
      {"f16", TKind::SIZE},
      {"f32", TKind::SIZE},
      {"f64", TKind::SIZE},
      {"f128", TKind::SIZE},
      {"u8", TKind::SIZE},
      {"u16", TKind::SIZE},
      {"u32", TKind::SIZE},
      {"u64", TKind::SIZE},
      {"u128", TKind::SIZE},
      {"c8", TKind::SIZE},
      {"c16", TKind::SIZE},
      {"c32", TKind::SIZE},
      {"as", TKind::CAST},
      {"frame", TKind::FRAME},
      {"override", TKind::OVERRIDE},
      {"async", TKind::ASYNC},
      {"world", TKind::WORLD},
      {"arena", TKind::ARENA},
      {"Handle", TKind::HANDLE},
      {"Error", TKind::ERROR},
      {"init", TKind::INIT},
      {"self", TKind::SELF},
      {"by", TKind::BY},
      {"onDestroy", TKind::ONDESTROY}};
};
