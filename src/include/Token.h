#pragma once

#include <string>
#include <unordered_map>

using namespace std;

enum class TKind {
  LEFT_PAREN,    //(
  RIGHT_PAREN,   //)
  LEFT_BRACE,    //{
  RIGHT_BRACE,   //}
  LEFT_BRACKET,  //[
  RIGHT_BRACKET, //]

  SEMICOLON, //;
  COLON,     //:
  COMMA,     //,
  DOT,       //.

  PLUS,        //+
  MINUS,       //-
  STAR,        //*
  DOUBLE_STAR, //**
  SLASH,       // /
  PERCENT,     // %

  EQUAL,             //=
  PLUS_EQUAL,        //+=
  MINUS_EQUAL,       //-=
  STAR_EQUAL,        // *=
  DOUBLE_STAR_EQUAL, //**=
  SLASH_EQUAL,       // /=
  PERCENT_EQUAL,     //%=

  DOUBLE_PLUS,  //++
  DOUBLE_MINUS, //--

  DOUBLE_EQUAL,  // ==
  BANG_EQUAL,    //!=
  LESS,          //<
  GREATER,       //>
  LESS_EQUAL,    //<=
  GREATER_EQUAL, //>=

  BANG, //!
  AND,  //&&
  OR,   //||

  QUESTION, //?

  INT,    // int
  FLOAT,  // float
  FIXED,  // fixed
  CHAR,   // char
  STRING, // string
  BOOL,   // bool
  NUL,    // null

  CONST,//const
  ROOT,//root

  LIT_INT,       // integer literal
  LIT_FLOAT,     // real number literal
  LIT_CHARACTOR, // charactor literal
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
  CASE,
  FOR,
  WHILE,

  BREAK,
  CONTINUE,
  RETURN,

  FUNC,
  VOID, // void

  CARET, //^
  BORROW,
  MUT,
  SHARE,
  WEAK,

  CLASS,
  STRUCT,
  ENUM,

  NEW,

  TRY,
  CATCH,

  ONEXIT,

  PUBLIC,
  PROTECTED,
  PRIVATE,
  INTERNAL,

  IMPL,
  TRAIT,
  EXTENDS,

  EMPTY,

};
struct Token{
  TKind kind=TKind::EMPTY;
  string text;
  int line,col;
};

