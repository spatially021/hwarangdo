#include "Parser.h"
#include "util/Error.h"
#include <memory>
#include <optional>

using Ptr = Stmt::Ptr;
using namespace std;

Ptr Parser::expressionStmt() {
  Token t = peek();
  Expr::Ptr expr = expression();
  consume(TKind::SEMICOLON, "expect ';' after expression statement");
  return make_shared<ExprStmt>(t, expr);
}

Ptr Parser::declStmt() {
  Token t = peek();
  Decl::Ptr decl = declaration(contexts.back());
  return make_shared<DeclStmt>(t, decl);
}

Ptr Parser::ifStmt() {
  Token t = peek();
  advance(); // if처리
  consume(TKind::LEFT_PAREN, "expect '(' after if");
  Expr::Ptr condition = expression();
  consume(TKind::RIGHT_PAREN, "expect ')' after condition");

  Ptr thenBranch = bodyStmt();
  Ptr elseBranch = nullptr;
  if (check(TKind::ELSE)) {
    advance(); // else 처리
    elseBranch = bodyStmt();
  }

  return make_shared<IfStmt>(t, condition, thenBranch, elseBranch);
}

Ptr Parser::blockStmt() {
  Token t = peek();
  vector<Ptr> s;
  while (!check(TKind::RIGHT_BRACE) && !isAtEnd()) {
    s.push_back(statement());
  }

  consume(TKind::RIGHT_BRACE, "expect '}' end of block");

  return make_shared<BlockStmt>(t, s);
}

Ptr Parser::bodyStmt() {
  ContextGuard _{contexts,BLOCK};
  if (check(TKind::LEFT_BRACE)) {
    advance(); //{처리
    return blockStmt();
  }
  return statement();
}

Ptr Parser::forStmt() {
  Token t = peek();
  advance(); // for 처리
  consume(TKind::LEFT_PAREN, "exepct '(' after for");

  Ptr init=declStmt();
  if(dynamic_pointer_cast<VarDecl>(init)->init!=nullptr) Error::diagnostic(t, "in for statement initiate expression not available");
  consume(TKind::SEMICOLON, "expect ';' after declare expression");
  Expr::Ptr from=expression();
  consume(TKind::DOUBLE_DOT, "range need '..'");
  Expr::Ptr to=expression();
  shared_ptr<Range> range=make_shared<Range>(from->token,from,to);
  Ptr body = bodyStmt();
  return make_shared<ForStmt>(t,init,range,body);
}

Ptr Parser::whileStmt() {
  Token t = peek();
  advance(); // while처리
  consume(TKind::LEFT_PAREN, "expect '(' after while");
  Expr::Ptr conditon = expression();
  consume(TKind::RIGHT_PAREN, "expect ')' after condition");
  Ptr body = bodyStmt();
  return make_shared<WhileStmt>(t, conditon, body);
}

Ptr Parser::switchStmt() {
  Token t = peek();
  advance(); // switch처리

  consume(TKind::LEFT_PAREN, "expect '(' after swtich");
  Expr::Ptr value = expression();
  consume(TKind::RIGHT_PAREN, "expect ')'");
  consume(TKind::LEFT_BRACE, "expect '{' after '(' in switch statement");
  vector<shared_ptr<Case>> cases;
  while (!check(TKind::RIGHT_BRACE) && !isAtEnd()) {
    Token tok = peek();
    if (check(TKind::CASE)) {
      advance(); // case 처리
      vector<Expr::Ptr> values;
      while (!check(TKind::EQAUL_AGNLEBUCKET) && !isAtEnd()) {
        values.push_back(expression());
        if (check(TKind::COMMA)) {
          if (check(TKind::EQAUL_AGNLEBUCKET, 1))
            Error::diagnostic(following(), "expect expression");
          else
            advance(); //,처리
        }
      }
      consume(TKind::EQAUL_AGNLEBUCKET, "expect '=>' after condition(s)");
      Ptr body;
      if (check(TKind::LEFT_BRACE)) {
        body = bodyStmt();
      } else
        Error::diagnostic(peek(), "expect '{' after '=>'");
      cases.push_back(make_shared<Case>(tok, values, body));
    } else if (check(TKind::DEFAULT)) {
      advance(); // default 처리
      consume(TKind::EQAUL_AGNLEBUCKET, "expect '=>' after default");
      vector<Expr::Ptr> values;
      Ptr body;
      if (check(TKind::LEFT_BRACE))
        body = bodyStmt();
      else
        Error::diagnostic(peek(), "expect '{' after '=>'");
      cases.push_back(make_shared<Case>(tok, values, body, true));
    } else
      Error::diagnostic(peek(),
                        "in switch statement place only case or default");
  }

  return make_shared<SwitchStmt>(t, value, cases);
}

Ptr Parser::returnStmt() {
  Token t = peek();
  advance(); // return 처리;
  Expr::Ptr expr;
  if (check(TKind::SEMICOLON))
    expr = nullptr;
  else
    expr = expression();
  consume(TKind::SEMICOLON, "expect ';' after return statement");
  return make_shared<ReturnStmt>(t, expr);
}

Ptr Parser::tryStmt() {
  Token t = peek();
  advance(); // try 처리
  Ptr body = bodyStmt();
  vector<shared_ptr<CatchClause>> catches;
  while (check(TKind::CATCH))
    catches.push_back(dynamic_pointer_cast<CatchClause>(catchStmt()));

  return make_shared<TryCatchStmt>(t, body, catches);
}

Ptr Parser::catchStmt(){
  Token t=peek();
  advance();//catch 처리
  consume(TKind::LEFT_BRACE, "expect '(' after catch");
  Token errorType=consume(TKind::IDENTIFIER, "expect error type after '('");
  optional<string> name=nullopt;
  if(!check(TKind::RIGHT_BRACE)) name=consume(TKind::IDENTIFIER, "expect error identifier after error type").text;
  
  consume(TKind::RIGHT_BRACE, "expect ')' end of catch()");
  Ptr body=bodyStmt();

  TypeNode::Ptr type=typeNodeConvertor(errorType);

  return make_shared<CatchClause>(t, type, name, body);
}

Ptr Parser::onexitStmt(){
  Token t=peek();
  advance();//onexit 처리
  consume(TKind::LEFT_BRACE, "expect '{' after onexit");
  Ptr body=blockStmt();
  return make_shared<OnexitStmt>(t,body); 
}

Ptr Parser::throwStmt(){
  Token t=peek();
  advance();//throw 처리
  Expr::Ptr expr=expression();
  return make_shared<ThrowStmt>(t,expr);
}