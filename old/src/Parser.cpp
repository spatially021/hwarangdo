#include "include/Parser.h"
#include "include/AST/Decl.h"
#include "include/Token.h"
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <regex>
#include <stdexcept>
#include <variant>
#include <vector>

using namespace std;

using Ptr = Stmt::Ptr;

bool hasMain = false;
// ────────────────────────────────
// 생성자
Parser::Parser(const vector<Token> &tokens) : tokens(tokens) {}

// ────────────────────────────────
// 메인 진입점
vector<Ptr> Parser::parse() {
  while (!isAtEnd()) {
    if (check(TokKind::CLASS)) {
      auto temp = declaration();
      statements.push_back(temp);
      if (auto cls = dynamic_cast<ClassDecl *>(temp.get())) {
        if (cls->name == "Main") {
          hasMain = true;
        }
      }
    } else {

      Token t = peek();
      error(t, "Only 'class' declarations are allowed at the top level.");
      advance(); // 에러 복구: 토큰을 하나 넘겨서 무한 루프 방지
    }
  }

  if (hasMain) {
    for (auto const &c : statements) {
      if (auto cls = dynamic_cast<ClassDecl *>(c.get())) {
        if (cls->name == "Main") {
          program = make_shared<Program>(statements, c);
          break;
        }
      }
    }
  } else
    program = make_shared<Program>(statements);

  return statements;
}

// ────────────────────────────────
// declaration → varDecl | functionDecl | statement
Ptr Parser::declaration() {

  if (match({TokKind::CLASS})) {
    return classDecl();
  }
  if (isType()) {
    if (isFunc())
      return functionDecl();
    else
      // 기존: consume(peek().kind, "")
      return varDecl();
  }
  if (check(TokKind::F_VOID)) {
    return functionDecl();
  }
  if (check(TokKind::FUNC)) {
    return functionDecl(true);
  }
  error(peek(), "Only declarations are allowed here.");
  return nullptr;
}

Ptr Parser::classDecl() {
  Token name = consume(TokKind::IDENTIFIER, "expect class name after 'class'.");

  consume(TokKind::LEFT_BRACE, "expect '{' before class body.");

  std::vector<Ptr> members;
  while (!isAtEnd() && !check(TokKind::RIGHT_BRACE)) {
    members.push_back(declaration());
  }
  consume(TokKind::RIGHT_BRACE, "expect '}' after class body.");

  Parser::types.insert(name.text);
  return std::make_shared<ClassDecl>(name.text, members);
}

Ptr Parser::functionDecl(bool isDynamic) {
  Token fType = advance();
  Token name =
      consume(TokKind::IDENTIFIER, "expect function name after return type.");
  consume(TokKind::LEFT_PAREN, "expect '(' after function name.");

  vector<shared_ptr<FuncDecl::Param>> params;
  if (!check(TokKind::RIGHT_PAREN)) {
    do {
      if (!isType()) {
        error(peek(), "expect parameter type before parameter name.");
        throw runtime_error("Invalid parameter type");
      }
      Token type = advance();
      Token parName =
          consume(TokKind::IDENTIFIER, "expect parameter name after type.");

      Token value;
      if (check(TokKind::EQUAL)) {
        consume(TokKind::EQUAL, "expect '=' before default parameter value.");
        switch (type.kind) {
        case TokKind::KW_FLOAT:
          if (check(TokKind::FLOAT)) {
            value = consume(TokKind::FLOAT,
                            "expect float literal as default value.");
            break;
          }
          break;
        case TokKind::KW_INT:
          if (check(TokKind::INTEGER)) {
            value = consume(TokKind::INTEGER,
                            "expect integer literal as default value.");
            break;
          } else {
            error(peek(), "unexpected default value for int parameter.");
            throw runtime_error("Invalid initial value");
          }
          break;
        case TokKind::KW_STRING:
          if (check(TokKind::STRING)) {
            value = consume(TokKind::STRING,
                            "expect string literal as default value.");
            break;
          }
          break;
        case TokKind::KW_CHAR:
          if (check(TokKind::CHAR)) {
            value =
                consume(TokKind::CHAR, "expect char literal as default value.");
            break;
          } else {
            error(peek(), "unexpected default value for char parameter.");
            throw runtime_error("Invalid initial value");
          }
          break;
        case TokKind::KW_BOOLEAN:
          if (check(TokKind::BOOLEAN)) {
            value = consume(TokKind::BOOLEAN,
                            "expect boolean literal as default value.");
            break;
          } else {
            error(peek(), "unexpected default value for boolean parameter.");
            throw runtime_error("Invalid initial value");
          }
          break;
        default:
          error(peek(), "unexpected token in parameter default value.");
        }
      } else
        value.text = "null";

      params.push_back(make_shared<FuncDecl::Param>(
          FuncDecl::Param{parName.text, make_shared<TypeNode>(type.text),
                          LiteralExpr(value.text, value.kind)}));

      if (check(TokKind::COMMA))
        consume(TokKind::COMMA, "expect ',' between parameters.");

    } while (!check(TokKind::RIGHT_PAREN));
  }

  consume(TokKind::RIGHT_PAREN, "expect ')' after parameters.");
  consume(TokKind::LEFT_BRACE, "expect '{' before function body.");

  vector<shared_ptr<Stmt>> m;
  while (!check(TokKind::RIGHT_BRACE))
    m.push_back(statement());

  consume(TokKind::RIGHT_BRACE, "expect '}' after function body.");

  return make_shared<FuncDecl>(name.text, params,
                               make_shared<TypeNode>(fType.text), m);
}

Ptr Parser::varDecl() {
  Token vType = advance();
  bool isSigned = true;
  bool isFixed = false;
  int size=-1;
  pair<int, int> sizeF={-1,-1};

  if (check(TokKind::COLON)) {
    advance();
    Token sizeT = advance();
    if (isValidSize(sizeT.text)) {
      std::regex fixedPattern(R"(^\d+\.\d+$)");
      std::regex unsignedPattern(R"(^[uU]\d+$)");
      string str = sizeT.text;

      std::smatch match;

      if (std::regex_match(str, match, fixedPattern)) {
        if(vType.text!="fixed") 
          throw runtime_error("this expression only use in fixed type");
        
        isFixed = true;
        int totalBits = std::stoi(match[1].str());
        int fractionBits = std::stoi(match[2].str());

        if (totalBits <= fractionBits) {
          throw std::runtime_error(
              "Total bits must be greater than fraction bits.");
        }
      } else if (std::regex_match(str, unsignedPattern)) {
        if (vType.text == "fixed")
          throw runtime_error("cannot allow this expression in fiex type");

        isSigned = false;
        size = std::stoi(str.substr(1));
      }
    } else
      error(sizeT, "invalid type size specifier");
  }

  Token name = consume(TokKind::IDENTIFIER, "expect variable name after type.");
  Expr::Ptr expr = nullptr;

  // 배열 선언 처리
  if (check(TokKind::LEFT_BRACKET)) {
    advance(); // '[' 소모
    Expr::Ptr sizeP = expression();
    consume(TokKind::RIGHT_BRACKET, "expect ']' after array size");

    // 배열 초기화 처리
    if (check(TokKind::EQUAL)) {
      consume(TokKind::EQUAL, "expect '=' before initializer expression");
      expr = expression();
    }

    consume(TokKind::SEMICOLON, "expect ';' after array declaration");

    if (isFixed) {
      return make_shared<ArrayDecl>(make_shared<FixedNode>(vType.text, sizeF),
                                    name.text, sizeP,
                                    expr // 초기화 값 전달
      );
    }
    return make_shared<ArrayDecl>(
        make_shared<SimpleTypeNode>(vType.text, size, !isSigned), name.text,
        sizeP,
        expr // 초기화 값 전달
    );
  }

  // 일반 변수 초기화 처리
  if (check(TokKind::EQUAL)) {
    consume(TokKind::EQUAL, "expect '=' before initializer expression");
    expr = expression();
  }

  consume(TokKind::SEMICOLON, "expect ';' after variable declaration");
  if (isFixed)
    return make_shared<VarDecl>(make_shared<FixedNode>(vType.text, sizeF),
                                name.text,expr,vType.kind);

  return make_shared<VarDecl>(
      make_shared<SimpleTypeNode>(vType.text, size, !isSigned), name.text, expr,
      vType.kind);
}

// --------------------------
// Expression entry point
// --------------------------
Expr::Ptr Parser::expression() { return assignment(); }

// --------------------------
// Assignment (=, +=, -=, etc.)
// --------------------------
Expr::Ptr Parser::assignment() {
  Expr::Ptr expr = ternary();

  if (match({TokKind::EQUAL, TokKind::PLUS_EQUAL, TokKind::MINUS_EQUAL,
             TokKind::STAR_EQUAL, TokKind::SLASH_EQUAL, TokKind::PERCENT_EQUAL,
             TokKind::DOUBLE_STAR_EQUAL})) {
    Token op = previous();
    Expr::Ptr value = assignment();

    // 왼쪽이 변수인지 검사
    if (auto var = std::dynamic_pointer_cast<VarExpr>(expr)) {
      return std::make_shared<AssignExpr>(var, op.text, value);
    }
    error(op, "Invalid assignment target.");
  }

  return expr;
}

// --------------------------
// Ternary (조건식 a ? b : c)
// --------------------------
Expr::Ptr Parser::ternary() {
  Expr::Ptr expr = logicalOr();

  if (match({TokKind::QUESTION})) {
    Expr::Ptr thenBranch = expression();
    consume(TokKind::COLON,
            "expect ':' after true branch of ternary expression.");
    Expr::Ptr elseBranch = expression();
    expr = std::make_shared<TernaryExpr>(expr, thenBranch, elseBranch);
  }

  return expr;
}

// --------------------------
// Logical OR (||)
// --------------------------
Expr::Ptr Parser::logicalOr() {
  Expr::Ptr expr = logicalAnd();

  while (match({TokKind::OR})) {
    Token op = previous();
    Expr::Ptr right = logicalAnd();
    expr = std::make_shared<BinaryExpr>(expr, op.text, right);
  }
  return expr;
}

// --------------------------
// Logical AND (&&)
// --------------------------
Expr::Ptr Parser::logicalAnd() {
  Expr::Ptr expr = equality();

  while (match({TokKind::AND})) {
    Token op = previous();
    Expr::Ptr right = equality();
    expr = std::make_shared<BinaryExpr>(expr, op.text, right);
  }
  return expr;
}

// --------------------------
// Equality (==, !=)
// --------------------------
Expr::Ptr Parser::equality() {
  Expr::Ptr expr = comparison();

  while (match({TokKind::EQUAL_EQUAL, TokKind::EXCLAIM_EQUAL})) {
    Token op = previous();
    Expr::Ptr right = comparison();
    expr = std::make_shared<BinaryExpr>(expr, op.text, right);
  }
  return expr;
}

// --------------------------
// Comparison (<, >, <=, >=)
// --------------------------
Expr::Ptr Parser::comparison() {
  Expr::Ptr expr = term();

  while (match({TokKind::LESS, TokKind::LESS_EQUAL, TokKind::GREATER,
                TokKind::GREATER_EQUAL})) {
    Token op = previous();
    Expr::Ptr right = term();
    expr = std::make_shared<BinaryExpr>(expr, op.text, right);
  }
  return expr;
}

// --------------------------
// Term (+, -)
// --------------------------
Expr::Ptr Parser::term() {
  Expr::Ptr expr = factor();

  while (match({TokKind::PLUS, TokKind::MINUS})) {
    Token op = previous();
    Expr::Ptr right = factor();
    expr = std::make_shared<BinaryExpr>(expr, op.text, right);
  }
  return expr;
}

// --------------------------
// Factor (*,**, /, %)
// --------------------------
Expr::Ptr Parser::factor() {
  Expr::Ptr expr = unary();

  while (match({TokKind::STAR, TokKind::SLASH, TokKind::PERCENT,
                TokKind::DOUBLE_STAR})) {
    Token op = previous();
    Expr::Ptr right = unary();
    expr = std::make_shared<BinaryExpr>(expr, op.text, right);
  }
  return expr;
}

// --------------------------
// Unary (!, -, ++, --)
// --------------------------
Expr::Ptr Parser::unary() {
  if (match({TokKind::EXCLAIM, TokKind::MINUS, TokKind::DOUBLE_PLUS,
             TokKind::DOUBLE_MINUS})) {
    Token op = previous();
    Expr::Ptr right = unary();
    return std::make_shared<UnaryExpr>(op.text, right);
  }
  return postfix();
}

// --------------------------
// Postfix (++ / -- / 함수 호출 / 배열 접근)
// --------------------------
Expr::Ptr Parser::postfix() {
  Expr::Ptr expr = primary();

  while (true) {
    if (match({TokKind::DOUBLE_PLUS, TokKind::DOUBLE_MINUS})) {
      Token op = previous();
      expr = std::make_shared<PostfixExpr>(expr, op.text);
    } else if (match({TokKind::LEFT_PAREN})) {
      // 함수 호출
      std::vector<Expr::Ptr> args;
      if (!check(TokKind::RIGHT_PAREN)) {
        do {
          args.push_back(expression());
        } while (match({TokKind::COMMA}));
      }
      consume(TokKind::RIGHT_PAREN, "expect ')' after function arguments.");
      expr = std::make_shared<CallExpr>(expr, args);
    } else if (match({TokKind::LEFT_BRACKET})) {
      // 배열 접근
      Expr::Ptr index = expression();
      consume(TokKind::RIGHT_BRACKET, "expect ']' after index expression.");
      expr = std::make_shared<ArrayAccessExpr>(expr, index);
    } else if (match({TokKind::DOT})) {
      Token memberName =
          consume(TokKind::IDENTIFIER, "expect member name after '.'");

      // 멤버 접근 기본 형태
      Expr::Ptr member = std::make_shared<AccessExpr>(expr, memberName);

      // 멤버가 함수 호출일 수도 있으니, 후속 postfix 패턴을 다시 처리
      expr = postfixAfterMember(member);
    }

    else
      break;
  }
  return expr;
}

Expr::Ptr Parser::postfixAfterMember(Expr::Ptr expr) {
  while (true) {
    if (match({TokKind::DOUBLE_PLUS, TokKind::DOUBLE_MINUS})) {
      Token op = previous();
      expr = std::make_shared<PostfixExpr>(expr, op.text);
    } else if (match({TokKind::LEFT_PAREN})) { // 함수 호출
      std::vector<Expr::Ptr> args;
      if (!check(TokKind::RIGHT_PAREN)) {
        do {
          args.push_back(expression());
        } while (match({TokKind::COMMA}));
      }
      consume(TokKind::RIGHT_PAREN, "expect ')' after function arguments.");
      expr = std::make_shared<CallExpr>(expr, args);
    } else if (match({TokKind::LEFT_BRACKET})) { // 배열 접근
      Expr::Ptr index = expression();
      consume(TokKind::RIGHT_BRACKET, "expect ']' after index expression.");
      expr = std::make_shared<ArrayAccessExpr>(expr, index);
    } else if (match({TokKind::DOT})) { // 중첩 멤버 접근 (a.b.c)
      Token memberName =
          consume(TokKind::IDENTIFIER, "expect member name after '.'");

      // 멤버 접근 기본 형태
      Expr::Ptr member = std::make_shared<AccessExpr>(expr, memberName);

      // 멤버가 함수 호출일 수도 있으니, 후속 postfix 패턴을 다시 처리
      expr = postfixAfterMember(member);
    } else
      break;
  }
  return expr;
}

// --------------------------
// Primary (리터럴, 변수, 괄호)
// --------------------------
Expr::Ptr Parser::primary() {
  if (match({TokKind::BOOLEAN, TokKind::INTEGER, TokKind::FLOAT, TokKind::CHAR,
             TokKind::STRING})) {
    return std::make_shared<LiteralExpr>(previous().text, previous().kind);
  }
  if (match({TokKind::IDENTIFIER})) {
    return std::make_shared<VarExpr>(previous().text);
  }
  if (match({TokKind::LEFT_PAREN})) {
    Expr::Ptr expr = expression();
    consume(TokKind::RIGHT_PAREN, "expect ')' after expression.");
    return std::make_shared<GroupExpr>(expr);
  }

  error(peek(), "expect expression.");
  return nullptr;
}

Ptr Parser::expressionStmt() {
  Expr::Ptr ptr = expression();
  consume(TokKind::SEMICOLON, "expect ';' after expresstion");
  return make_shared<ExprStmt>(ptr);
}

Ptr Parser::bodyStmt() {
  std::vector<Stmt::Ptr> statements;
  if (check(TokKind::LEFT_BRACE)) {
    advance(); // consume '{'
    while (!check(TokKind::RIGHT_BRACE) && !isAtEnd()) {
      auto stmt = statement();
      if (stmt)
        statements.push_back(stmt);
    }
    consume(TokKind::RIGHT_BRACE, "expect '}' after block.");
  } else {
    auto stmt = statement();
    if (stmt)
      statements.push_back(stmt);
  }
  return make_shared<BlockStmt>(statements);
}

Ptr Parser::ifStmt() {
  advance(); // consume 'if'
  Expr::Ptr expr;

  consume(TokKind::LEFT_PAREN, "expect '(' after 'if'.");
  expr = expression();
  consume(TokKind::RIGHT_PAREN, "expect ')' after condition.");

  Ptr body = bodyStmt();

  if (check(TokKind::ELSE)) {
    advance(); // consume 'else'
    return make_shared<IfStmt>(expr, body, bodyStmt());
  }
  return make_shared<IfStmt>(expr, body);
}

Ptr Parser::forStmt() {
  advance();
  consume(TokKind::LEFT_PAREN, "expect '(' after 'for'.");

  variant<Stmt::Ptr, Expr::Ptr, nullptr_t> init;

  if (isType()) {
    Token type = consume(peek().kind, "");
    Token ident = consume(TokKind::IDENTIFIER, "");
    consume(TokKind::EQUAL, "");
    Expr::Ptr expr = expression();
    init = make_shared<VarDecl>(make_shared<TypeNode>(type.text), ident.text,
                                expr, type.kind);
  } else if (!check(TokKind::SEMICOLON))
    init = expression();
  consume(TokKind::SEMICOLON, "expect ';' in for statement specifier");
  Expr::Ptr condition = expression();
  consume(TokKind::SEMICOLON, "expect ';' in for statement specifier");
  Expr::Ptr increment = expression();
  consume(TokKind::RIGHT_PAREN, "expect ')' after for statement");
  Ptr body = bodyStmt();

  return make_shared<ForStmt>(init, condition, increment, body);
}

Ptr Parser::whileStmt() {
  advance();
  consume(TokKind::LEFT_PAREN, "expect '(' after while.");
  Expr::Ptr condition = expression();
  consume(TokKind::RIGHT_PAREN, "expect ')' after condition.");
  Ptr body = bodyStmt();

  return make_shared<WhileStmt>(condition, body);
}

Ptr Parser::returnStmt() {
  advance();
  if (check(TokKind::SEMICOLON)) {
    advance();
    return make_shared<ReturnStmt>();
  }

  Expr::Ptr expr = expression();

  consume(TokKind::SEMICOLON, "expect ';' after return statement");

  return make_shared<ReturnStmt>(expr);
}

Ptr Parser::switchStmt() {
  advance();
  consume(TokKind::LEFT_PAREN, "expect '(' after 'switch'");
  Expr::Ptr value = expression();
  consume(TokKind::RIGHT_PAREN, "expect ')' after condition");
  consume(TokKind::LEFT_BRACE, "expect '{' after '(' ");
  std::vector<std::shared_ptr<CaseStmt>> cases;
  while (!check(TokKind::RIGHT_BRACE)) {
    if (check(TokKind::CASE)) {
      advance();
      Expr::Ptr condition = expression();
      consume(TokKind::COLON, "expect ':' after case");
      vector<Stmt::Ptr> body;
      do {
        body.push_back(statement());
      } while (!(check(TokKind::RIGHT_BRACE) || check(TokKind::CASE) ||
                 check(TokKind::DEFAULT)));
      cases.push_back(make_shared<CaseStmt>(condition, body));
    } else if (check(TokKind::DEFAULT)) {
      advance();
      consume(TokKind::COLON, "expect ':' after default.");
      vector<Stmt::Ptr> body;
      while (!(check(TokKind::RIGHT_BRACE) || check(TokKind::CASE))) {
        body.push_back(statement());
      }
      cases.push_back(make_shared<CaseStmt>(nullptr, body));
    } else
      error(peek(), "unknown expression");
  }

  consume(TokKind::RIGHT_BRACE, "expect '}' after switch statement");

  return make_shared<SwitchStmt>(value, cases);
}

Ptr Parser::caseStmt() { return nullptr; }

Ptr Parser::statement() {
  switch (Parser::peek().kind) {
  case TokKind::IF:
    return ifStmt();
  case TokKind::FOR:
    return forStmt();
  case TokKind::IDENTIFIER:
    // TODO: 객체 선언 구현하기
  case TokKind::PLUS:
  case TokKind::MINUS:
  case TokKind::DOUBLE_PLUS:
  case TokKind::DOUBLE_MINUS:
  case TokKind::LEFT_PAREN:
  case TokKind::EXCLAIM:
  case TokKind::DOT:
    return expressionStmt();
  case TokKind::RIGHT_PAREN:
    error(peek(), "')' cannot place without '('");
    return nullptr;

  case TokKind::LEFT_BRACE:
    return bodyStmt();
  case TokKind::RIGHT_BRACE:
    error(peek(), "'}' cannot place without '{'");
    return nullptr;

  case TokKind::ELSE:
    error(peek(), "cannot place 'else' without if statement.");
    return nullptr;
  case TokKind::SEMICOLON:
    advance();
    return make_shared<EmptyStmt>();
  case TokKind::KW_INT:
  case TokKind::KW_FLOAT:
  case TokKind::KW_BOOLEAN:
  case TokKind::KW_CHAR:
  case TokKind::KW_STRING:
    return varDecl();
  case TokKind::WHILE:
    return whileStmt();
  case TokKind::RETURN:
    return returnStmt();
  case TokKind::SWITCH:
    return switchStmt();
  case TokKind::CASE:
    error(peek(), "case is not allowed without switch");
    return nullptr;
  case TokKind::FUNC:
    error(peek(), "function declare is not allowed here");
    return nullptr;
  case TokKind::CLASS:
    return classDecl();
  case TokKind::BREAK:
    advance();
    consume(TokKind::SEMICOLON, "expect ';' after break");
    return make_shared<BreakStmt>();
  case TokKind::CONTINUE:
    advance();
    consume(TokKind::SEMICOLON, "expect ';' after continue");
    return make_shared<ContinueStmt>();
  default:
    error(peek(), "expected expresstion");
    return nullptr;
  }
}

// ────────────────────────────────
// 유틸리티 함수들
bool Parser::match(std::initializer_list<TokKind> kinds) {
  for (auto kind : kinds) {
    if (check(kind)) {
      advance();
      return true;
    }
  }
  return false;
}

bool Parser::check(TokKind kind) const {
  if (isAtEnd())
    return false;
  return peek().kind == kind;
}

const Token &Parser::advance() {
  if (!isAtEnd())
    current++;
  return previous();
}

const Token &Parser::peek() const {
  if (current >= tokens.size()) {
    throw runtime_error(("Peek out of range"));
  }

  return tokens[current];
}

const Token &Parser::previous() const {
  if (current == 0)
    throw runtime_error("No previous token");
  return tokens[current - 1];
}

bool Parser::isAtEnd() const {
  return current >= tokens.size() || peek().kind == TokKind::END;
}

const Token &Parser::consume(TokKind kind, const string &message) {
  if (check(kind))
    return advance();
  error(peek(), message);
  throw runtime_error("adsf");
}

void Parser::error(const Token &token, const string &message) {
  string m = "[line ";
  m += (token.line);
  m += "] Error at '" + token.text + "': " + message;

  throw runtime_error(m);
}

bool Parser::isFunc() const {
  return (current + 2 < tokens.size() &&
          tokens[current + 1].kind == TokKind::IDENTIFIER &&
          tokens[current + 2].kind == TokKind::LEFT_PAREN);
}

bool Parser::isType() const {
  switch (Parser::peek().kind) {
  case TokKind::KW_INT:
  case TokKind::KW_BOOLEAN:
  case TokKind::KW_CHAR:
  case TokKind::KW_FLOAT:
  case TokKind::KW_STRING:
    return true;
  case TokKind::IDENTIFIER:
    return Parser::types.find(peek().text) != Parser::types.end();
  default:
    return false;
  }
}

bool Parser::isValidSize(const std::string &s) const {
  // signed integer: 64
  std::regex signedPattern(R"(^\d+$)");
  // unsigned integer: u32
  std::regex unsignedPattern(R"(^[uU]\d+$)");
  // fixed-point: xx.xx (예: 16.8, 32.16)
  std::regex fixedPattern(R"(^\d+\.\d+$)");

  return std::regex_match(s, signedPattern) ||
         std::regex_match(s, unsignedPattern) ||
         std::regex_match(s, fixedPattern);
}