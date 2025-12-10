#pragma once

#include "include/Parser.h"
#include "include/AST/Decl.h"
#include "include/Token.h"
#include <stdexcept>
#include <vector>

using Ptr = shared_ptr<ASTNode>;

Parser::Parser(const vector<Token> &tokens):tokens(tokens){}

vector<Ptr> Parser::parse(){
    while (!isAtEnd()) {
      if (check({TKind::CLASS, TKind::STRUCT, TKind::IMPL, TKind::TRAIT})) {
        auto stmt=declaration();
        statements.push_back(stmt);
      } else if (isAccessModifier()) {
          if(following().kind==TKind::CLASS||following().kind==TKind::STRUCT||following().kind==TKind::IMPL||following().kind==TKind::TRAIT){
            auto stmt=declaration();
            statements.push_back(stmt);
          }else{
            error(peek(), "not allowed expression");
          }
      } else {
        error(peek(), "not allowed expression");
      }
    }

    return statements;
}

Decl::Ptr Parser::declaration() {
  if(isAccessModifier()){
    if (check(TKind::CLASS),1)
      return classDecl();

    //   if (check(TKind::STRUCT),1)
    //     return structDecl(modi);

    //   if (check(TKind::IMPL),1)
    //     return implDecl(modi);

    //   if (check(TKind::TRAIT),1)
    //     return traitDecl(modi);

    //   if (isType()) {
    //     if (isFunc())
    //       return functionDecl(modi);
    //     else
    //       return varDecl(modi);
    //   }

    //   if (check(TKind::VOID))
    //     return functionDecl(modi);
    //   if (check(TKind::FUNC))
    //     return functionDecl(modi, true);
    else error(following(), "expect 'class','struct','impl' or 'trait'");
  }else{
    if (check(TKind::CLASS))
      return classDecl();

    //   if (check(TKind::STRUCT))
    //     return structDecl(modi);

    //   if (check(TKind::IMPL))
    //     return implDecl(modi);

    //   if (check(TKind::TRAIT))
    //     return traitDecl(modi);

    //   if (isType()) {
    //     if (isFunc())
    //       return functionDecl(modi);
    //     else
    //       return varDecl(modi);
    //   }

    //   if (check(TKind::VOID))
    //     return functionDecl(modi);
    //   if (check(TKind::FUNC))
    //     return functionDecl(modi, true);
  }



  auto t = peek();
  error(t, "Only declarations are allowed here.");
  throw runtime_error("");
}

Stmt::Ptr Parser::statement(){

}

Expr::Ptr Parser::expression(){

}