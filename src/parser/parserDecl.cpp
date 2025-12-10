#pragma once

#include "../include/Parser.h"
#include <memory>
#include <regex>
#include <string>

using Ptr=Decl::Ptr;

Ptr Parser::classDecl() {
  AModifier modi=AModifier::DEFAULT;
  
  Token t;

  if(isAccessModifier()){
    modi = AModifierConvertor(peek());
    t=advance();
    advance();
  }else{
    t=advance();
  }

  Token name = consume(TKind::IDENTIFIER, "expect class name after 'class'.");
  optional<string> base;
  if (check(TKind::EXTENDS)){
    advance();
    base->append(advance().text);
  }

  vector<string> traits;
  if (check(TKind::COLON)){
    advance();
    while (!check(TKind::LEFT_BRACE)) {
      traits.push_back(advance().text);
    }
  }


  consume(TKind::LEFT_BRACE, "exepct '{' before class body.");
  vector<shared_ptr<ASTNode>> body;
  while (!check(TKind::RIGHT_BRACE) || isAtEnd()) {
    body.push_back(statement());
  }

  consume(TKind::RIGHT_BRACE, "expect '}' after class body");

  return make_shared<ClassDecl>(t, name.text, body, base, traits, modi);
}

Ptr Parser::structDecl() {

  Token t;
  AModifier modi = AModifier::DEFAULT;

  if (isAccessModifier()) {
    modi = AModifierConvertor(peek());
    t = advance();
    advance();
  } else {
    t = advance();
  }

  Token name=consume(TKind::IDENTIFIER, "expect struct name after 'struct'.");

  vector<string> traits;
  if(check(TKind::COLON)){
    advance();
    while (!check(TKind::LEFT_BRACE)) {
      traits.push_back(advance().text);
    }
  }

  consume(TKind::LEFT_BRACE,"expect '{' before struct body");
  vector<shared_ptr<VarDecl>> fields;

  while (!check(TKind::RIGHT_BRACE)) {
    if(isType()&&!isFunc()){
      auto var = dynamic_pointer_cast<VarDecl>(varDecl());
      if (var) {
        fields.push_back(var);
      }
    }
  }

  consume(TKind::RIGHT_BRACE, "expect '}' after struct body");

  return make_shared<StructDecl>(t,name.text,fields,traits,modi);
}

Ptr Parser::varDecl(){
  Token t;
  Token ty;
  AModifier modi = AModifier::DEFAULT;

  if (isAccessModifier()) {
    modi = AModifierConvertor(peek());
    t = advance();
    ty=advance();
  } else {
    t = advance();
    ty=t;
  }

  Token name=consume(TKind::IDENTIFIER, "expect var name after type-keyword");

  int size=-1;
  pair<int,int> fixedSize={-1,-1};
  bool isSigned=true;

  if(check(TKind::COLON)){
    static const std::regex signedPattern(R"(^(\d+)$)");
    static const std::regex unsignedPattern(R"(^[uU](\d+)$)");
    static const std::regex fixedPattern(R"(^(\d+)\.(\d+)$)");
    static const std::regex unsignedFixedPattern(R"(^[uU](\d+)\.(\d+)$)");

    if(ty.kind==TKind::IDENTIFIER) error(peek(), "':' is only allowed built-in types");
    advance();
    if(isValidSize(peek().text)){
      string s=peek().text;
      std::smatch match;

      if (ty.kind == TKind::FIXED) {
        if(std::regex_match(s,match,fixedPattern)){
          fixedSize = { std::stoi(match[1].str()), std::stoi(match[2].str())};
        }else if(std::regex_match(s,match,unsignedFixedPattern)){
          isSigned=false;
          fixedSize = {std::stoi(match[1].str()), std::stoi(match[2].str())};
        }else error(peek(), "invalid size");
      } else {  
        if(std::regex_match(s,match,signedPattern)){
          size=std::stoi(match[1].str());
        }else if(std::regex_match(s,match,unsignedPattern)){
          isSigned=false;
          size = std::stoi(match[1].str());
        }else
          error(peek(), "invalid size");
      }
    }else error(peek(), "invalid size");
  }
  
  //TODO: 초기화 최초 초기화 부분 작성 필요.

}