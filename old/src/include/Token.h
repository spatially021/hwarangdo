#pragma once
#include <string>

using namespace std;

/*
define token kind
*/
enum class TokKind {
  LEFT_PAREN,    // (
  RIGHT_PAREN,   // )
  LEFT_BRACE,    // {
  RIGHT_BRACE,   // }
  LEFT_BRACKET,  // [
  RIGHT_BRACKET, // ]

  SEMICOLON,    // ;
  COLON,        // :
  COMMA,        // ,
  DOT,          // .
  PLUS,         // +
  DOUBLE_PLUS,  //++
  MINUS,        // -
  DOUBLE_MINUS, //--
  STAR,         // *
  DOUBLE_STAR,  //**
  STAR_SLASH,   // */
  SLASH,        // /
  DOUBLE_SLASH, // //
  SLASH_STAR,   // /*
  PERCENT,      // %
  QUESTION,     //?

  EQUAL,             // =
  PLUS_EQUAL,        //+=
  MINUS_EQUAL,       //-=
  STAR_EQUAL,        //*=
  DOUBLE_STAR_EQUAL, // **=
  SLASH_EQUAL,       // /=
  PERCENT_EQUAL,     //%=

  EQUAL_EQUAL,   // ==
  EXCLAIM,       // !
  EXCLAIM_EQUAL, //!=
  GREATER,       // >
  GREATER_EQUAL, // >=
  LESS,          // <
  LESS_EQUAL,    // <=

  IDENTIFIER, // [a-z A-Z _][a-z A-Z _]* VAR NAME, CLASS NAME ETC

  BOOLEAN, // true, false value
  INTEGER, // INTEGER
  FLOAT,   // FLOAT NUMBER

  CHAR,   // CHARACTER
  STRING, // STRING

  KW_NULL, // null

  END, // \0

  IF,   // if
  ELSE, // else
  AND,  // &&
  OR,   // ||

  KW_INT,   // int
  KW_FLOAT, // float
  KW_FIXED,
  KW_BOOLEAN, // boolean
  KW_CHAR,    // char
  KW_STRING,  // string

  FOR,      // for
  WHILE,    // while
  RETURN,   // return
  SWITCH,   // switch
  CASE,     // case
  BREAK,    // break
  CONTINUE, // continue
  DEFAULT,  // default

  F_INT,
  F_FlOAT,
  F_DOUBLE,
  F_BOOLEAN,
  F_CHAR,
  F_STRING,
  F_VOID,
  FUNC, // func

  CLASS, // class 
  STRUCT,//struct

  EMPTY,

};

/*
define token struct
*/
struct Token {
  TokKind kind = TokKind::KW_NULL;
  string text;
  int line, col;
};
