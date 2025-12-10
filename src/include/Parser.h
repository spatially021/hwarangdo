#pragma once

#include "AST/ASTNode.h"
#include "AST/Decl.h"
#include "AST/Stmt.h"
#include "Token.h"
#include <cstddef>
#include <memory>
#include <unordered_set>
#include <vector>

using namespace std;

class Parser{
public:
explicit Parser(const vector<Token> &tokens);
vector<shared_ptr<ASTNode>> statements;

vector<shared_ptr<ASTNode>> parse();

private:

const std::vector<Token> &tokens;
size_t current = 0;

// 문장 단위
Decl::Ptr declaration(); // 변수 선언, 함수 선언 등
Stmt::Ptr statement();   // 일반 문장 (if, while, return, expression 등)

// 표현식 단위
Expr::Ptr expression();

Expr::Ptr assignment();
Expr::Ptr logicalOr();
Expr::Ptr logicalAnd();
Expr::Ptr equality();
Expr::Ptr comparison();
Expr::Ptr term();
Expr::Ptr factor();
Expr::Ptr unary();
Expr::Ptr primary();
Expr::Ptr postfix();
Expr::Ptr ternary();
Expr::Ptr conditionExpr();

// 함수 및 블록
Stmt::Ptr expressionStmt();
Stmt::Ptr varStmt();
Stmt::Ptr blockStmt();
Stmt::Ptr ifStmt();
Stmt::Ptr forStmt();
Stmt::Ptr whileStmt();
Stmt::Ptr switchStmt();
Stmt::Ptr caseStmt();
Stmt::Ptr returnStmt();
Stmt::Ptr tryStmt();
Stmt::Ptr catchStmt();
Stmt::Ptr onexitStmt();

Decl::Ptr classDecl();
Decl::Ptr structDecl();
Decl::Ptr implDecl();
Decl::Ptr traitDecl();
Decl::Ptr enumDecl();
Decl::Ptr functionDecl(bool isDynamic = false);
Decl::Ptr varDecl();

// 유틸리티
bool match(std::initializer_list<TKind> kinds);
bool check(TKind kind, size_t step = 0) const;
bool check(std::initializer_list<TKind> kind, size_t step = 0) const;
const Token &advance();
const Token &peek() const;
const Token &previous() const;
const Token &following(size_t step = 1) const;
bool isAtEnd() const;
const Token &consume(TKind kind, const std::string &message);
[[noreturn]]
void error(const Token &token, const std::string &message) const;
bool isFunc() const;
bool isType() const;
Token parseLiteralForType(const Token &type);
bool isValidSize(const std::string &s) const;
bool isAccessModifier() const;
bool isTypeToken(TKind k) const ;
AModifier AModifierConvertor(Token t);
};