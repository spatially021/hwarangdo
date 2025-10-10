#pragma once
#include "Token.h"
#include <string>
#include <vector>

using namespace std;

/*
lexer: convert source string to tokens
*/
// struct Lexer {
//     explicit Lexer(const std::string& src);
//     Token next();                          // get next token
//     std::vector<Token> tokenize();         // tokenize whole source
// private:
//     std::string src;
//     size_t i = 0;
//     int line = 1, col = 1;

//     char peek() const;
//     char get();
//     void skipWS();
//     bool isIdentFirst(char c);
//     bool isIdentRest(char c);
//     bool isDigit(char c);
// };

struct lexer{
    explicit Lexer(const string& src);
    Token next();
    vector<Token> tokenized();

private:
    string src;
    size_t i=0;
    int line,col;

    char peek() const;
    char get();
    void skipWS();
    bool isIdentFirst(char c);
    bool isIdentRest(char c);
    bool isNumber(char c);
}