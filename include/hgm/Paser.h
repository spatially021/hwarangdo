#pragma once
#include "Token.h"
#include "AST.h"
#include <vector>

/*
parser: convert tokens to AST
*/
struct Parser {
    explicit Parser(const std::vector<Token>& toks);

    Program parseProgram();

private:
    const std::vector<Token>& toks;
    size_t p = 0;

    const Token& peek() const;
    const Token& get();
    bool match(TokKind k);
    const Token& expect(TokKind k, const std::string& msg);

    // sub-parsers
    ClassDecl parseClass();
    FnDecl parseFn();
    Param parseParam();
    Block parseBlock();
    StmtPtr parseStmt();
    ExprPtr parseExpr();
};
