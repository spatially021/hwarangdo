#pragma once
#include <string>

/*
define token kind
*/
enum class TokKind {
    LEFT_PAREN, // (
    RIGTH_PAREN,// )
    LEFT_BRACE,// {
    RIGTH_BRACE, // }
    LEFT_BRACKET, // [
    RIGTH_BTACKET, // ]

    SEMICOLON, // ;
    COLON,// :
    COMMA, // ,
    DOT, // .
    PLUS, // +
    MINUS, // -
    STAR, // *
    SLASH, // /
    PERCENT, // %

    EQUAL, // =
    EQUAL_EQUAL, // ==
    EXCLAIM, // !
    EXCLAIM_EQUAL, //!=
    GREATER, // >
    GREATER_EQUAL, // >=
    LESS, // <
    LESS_EQUAL, // <=

    IDENTIFIER, // [a-z A-Z _][a-z A-Z _]* VAR NAME, CLASS NAME ETC

    BOOLEAN, //true, false value
    INTEGER, //I64 NUMBER
    FLOAT, //F32 REAL NUMBER
    DOUBLE, //D64 REAL NUMBER
    CHAR, //CHARACTER
    STRING, //STRING
    KW_NULL, //null

    END, // \0

    IF, // if
    ELSE, // else
    AND, // &&
    OR, // ||
    KW_INT, // int
    KW_FLOAT, // float
    KW_DOUBLE, // double
    KW_BOOLEAN, // boolean
    KW_CHAR, // char
    KW_STRING,// string
    FOR, // for
    WHILE, // while
    RETURN, // return
    SWITCH, // switch
    CASE, // case

    F_INT,
    F_FlOAT,
    F_DOUBLE,
    F_BOOLEAN,
    F_CHAR,
    F_STRING,
    FUNC //func
};

/*
define token struct
*/
struct Token {
    TokKind kind;
    string text;
    int line, col;
};
