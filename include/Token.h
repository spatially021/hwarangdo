#pragma once

#include <string>

using namespace std;

enum class TKind {
  LEFT_PAREN,    //(
  RIGHT_PAREN,   //)
  LEFT_BRACE,    //{
  RIGHT_BRACE,   //}
  LEFT_BRACKET,  //[
  RIGHT_BRACKET, //]

  SEMICOLON,  //;
  COLON,      //:
  COMMA,      //,
  DOT,        //.
  DOUBLE_DOT, //..

  PLUS,        //+
  MINUS,       //-
  STAR,        //*
  DOUBLE_STAR, //**
  SLASH,       // /
  PERCENT,     // %

  EQUAL,                           //=
  PLUS_EQUAL,                      //+=
  MINUS_EQUAL,                     //-=
  STAR_EQUAL,                      // *=
  DOUBLE_STAR_EQUAL,               //**=
  SLASH_EQUAL,                     // /=
  PERCENT_EQUAL,                   //%=
  CARET_EQUAL,                     // ^=
  AMPERSAND_EQAUL,                 //&=
  PIPE_EQUAL,                      //|=
  DOUBLE_ANGLEBUCKET_EQAUL,        //<<=
  DOUBLE_RIGHT_ANGLE_BUCKET_EQUAL, //>>=

  DOUBLE_EQUAL,  // ==
  BANG_EQUAL,    //!=
  LESS,          //<
  GREATER,       //>
  LESS_EQUAL,    //<=
  GREATER_EQUAL, //>=

  BANG, //!
  AND,  //&&
  OR,   //||

  UNDERBAR, //_

  QUESTION, //?

  INT,    // int
  FLOAT,  // float
  FIXED,  // fixed
  CHAR,   // char
  STRING, // string
  BOOL,   // bool
  NUL,    // null

  CONST, // const
  ROOT,  // root

  LIT_INT,       // integer literal
  LIT_FLOAT,     // real number literal
  LIT_CHARACTER, // charactor literal
  LIT_STRING,    // string literal
  LIT_BOOL,      // true,false

  SLASH_STAR,   // /*
  STAR_SLASH,   // */
  DOUBLE_SLASH, // //

  IDENTIFIER, // name of something

  END, //\0

  IF,
  ELSE,

  SWITCH,
  MATCH,
  CASE,
  DEFAULT,
  DOUBLE_ANGLEBUCKET, //<<
  EQAUL_AGNLEBUCKET,  //=>

  FOR,
  WHILE,

  BREAK,
  CONTINUE,
  RETURN,

  FUNC,
  VOID, // void

  CARET,                     //^
  AMPERSAND,                 //&
  TILDE,                     //~
  PIPE,                      //|
  DOUBLE_RIGHT_ANGLE_BUCKET, //>>

  CLASS,
  STRUCT,
  ENUM,

  NEW,

  TRY,
  CATCH,
  THROW,

  ONEXIT,

  PUBLIC,
  PROTECTED,
  PRIVATE,
  INTERNAL,

  IMPL,
  TRAIT,
  EXTENDS,

  SUPER,
  THIS,

  EMPTY,

  SIZE, // built-in type size token

};
struct Token {
  TKind kind = TKind::EMPTY;
  string text;
  int line, col;
};
