#pragma once

#include "../include/Parser.h"
#include <memory>
#include <optional>
#include <regex>
#include <string>

using Ptr = Decl::Ptr;

Ptr Parser::classDecl() {
  AModifier modi = AModifier::DEFAULT;

  Token t = peek();

  if (isAccessModifier())
    modi = AModifierConvertor(advance());

  advance(); // class 처리

  Token name = consume(TKind::IDENTIFIER, "expect class name after 'class'.");
  optional<string> base;
  if (check(TKind::EXTENDS)) {
    advance();//extends 처리
    base = advance().text;
  }

  vector<string> traits;
  if (check(TKind::COLON)) {
    advance();//:처리
    while (!check(TKind::LEFT_BRACE) && !isAtEnd()) {
      traits.push_back(advance().text);
    }
  }

  consume(TKind::LEFT_BRACE, "exepct '{' before class body.");
  vector<shared_ptr<ASTNode>> body;
  while (!check(TKind::RIGHT_BRACE) && !isAtEnd()) {
    body.push_back(statement());
  }

  consume(TKind::RIGHT_BRACE, "expect '}' after class body");

  return make_shared<ClassDecl>(t, name.text, body, base, traits, modi);
}

Ptr Parser::structDecl() {

  Token t = peek();
  AModifier modi = AModifier::DEFAULT;

  if (isAccessModifier())
    modi = AModifierConvertor(advance());

  advance(); // struct 처리

  Token name = consume(TKind::IDENTIFIER, "expect struct name after 'struct'.");
  vector<string> traits;
  if (check(TKind::COLON)) {
    advance();//:처리
    while (!check(TKind::LEFT_BRACE) && !isAtEnd()) {
      traits.push_back(advance().text);
    }
  }

  consume(TKind::LEFT_BRACE, "expect '{' before struct body");
  vector<shared_ptr<VarDecl>> fields;
  while (!check(TKind::RIGHT_BRACE) && !isAtEnd()) {
    if (isType() && !isFunc()) {
      auto var = dynamic_pointer_cast<VarDecl>(varDecl());
      if (var) {
        fields.push_back(var);
      } else
        error(peek(), "this expression is not allowed");
    } else
      error(peek(), "only var or instance declare here");
  }

  consume(TKind::RIGHT_BRACE, "expect '}' after struct body");

  return make_shared<StructDecl>(t, name.text, fields, traits, modi);
}

Ptr Parser::varDecl() {
  Token t = peek();
  Token ty;
  AModifier modi = AModifier::DEFAULT;
  bool isMutable = true;
  bool flag = false;
  if (isAccessModifier())
    modi = AModifierConvertor(advance());

  if (check(TKind::CONST)) {
    advance();//const 처리
    isMutable = false;
  }

  ty = advance();

  int size = -1;
  pair<int, int> fixedSize = {-1, -1};
  bool isSigned = true;

  if (check(TKind::COLON)) {
    static const std::regex signedPattern(R"(^(\d+)$)");
    static const std::regex unsignedPattern(R"(^[uU](\d+)$)");
    static const std::regex fixedPattern(R"(^(\d+)\.(\d+)$)");
    static const std::regex unsignedFixedPattern(R"(^[uU](\d+)\.(\d+)$)");

    if (ty.kind == TKind::IDENTIFIER)
      error(peek(), "':' is only allowed built-in types");
    advance();//:처리
    if (isValidSize(peek().text)) {
      string s = peek().text;
      std::smatch match;

      if (ty.kind == TKind::FIXED) {
        if (std::regex_match(s, match, fixedPattern)) {
          fixedSize = {std::stoi(match[1].str()), std::stoi(match[2].str())};
        } else if (std::regex_match(s, match, unsignedFixedPattern)) {
          isSigned = false;
          fixedSize = {std::stoi(match[1].str()), std::stoi(match[2].str())};
        } else
          error(peek(), "invalid size expression");
      } else {
        if (std::regex_match(s, match, signedPattern)) {
          size = std::stoi(match[1].str());
        } else if (std::regex_match(s, match, unsignedPattern)) {
          isSigned = false;
          size = std::stoi(match[1].str());
        } else
          error(peek(), "invalid size expression");
      }
    } else
      error(peek(), "invalid size expression");
  }

  Token name = consume(TKind::IDENTIFIER, "expect var name after type-keyword");

  if (check(TKind::LEFT_BRACKET)) {
    advance();//[처리
    Expr::Ptr size = expression();
    consume(TKind::RIGHT_BRACKET, "expect ']' after array's size expression");

    Expr::Ptr init = nullptr;

    if (check(TKind::EQUAL)) {
      advance();//=처리
      init = expression();
    }

    consume(TKind::SEMICOLON, "expect ';' after expression");

    TypeNode::Ptr node = typeNodeConvertor(ty);

    auto aNode = make_shared<ArrayTypeNode>(t, node, size);

    return make_shared<ArrayDecl>(t, name.text, aNode, init, isMutable, modi);
  }

  Expr::Ptr init = nullptr;

  if (check(TKind::EQUAL)) {
    advance();//=처리
    init = expression();
  }

  consume(TKind::SEMICOLON, "expect ';' after expression.");
  TypeNode::Ptr node = typeNodeConvertor(ty);
  return make_shared<VarDecl>(t, name.text, node, init, isMutable, modi);
}

Ptr Parser::functionDecl(bool isDynamic) {
  Token t;
  Token ty;
  AModifier modi = AModifier::DEFAULT;
  if (isAccessModifier()) {
    modi = AModifierConvertor(peek());
    t = advance();

  } else
    t = peek();

  ty = advance();

  Token name = consume(TKind::IDENTIFIER, "expect function's name");

  consume(TKind::LEFT_PAREN, "expect '(' after function name");

  vector<shared_ptr<FuncDecl::Param>> params;

  while (!check(TKind::RIGHT_PAREN) && !isAtEnd()) {
    if (isType()) {
      Token type = advance();
      Token name = consume(TKind::IDENTIFIER,
                           "expect parameter name after parameter type");
      optional<Expr::Ptr> init;
      if (check(TKind::EQUAL)) {
        advance();//=처리
        init = expression();
      }
      TypeNode::Ptr returnType = typeNodeConvertor(type);
      params.push_back(
          make_shared<FuncDecl::Param>(name.text, returnType, init));
      if (check(TKind::COMMA)) {
        if (!check(TKind::RIGHT_PAREN, 1))
          advance();//,처리
        else
          error(following(), "after ',' need more parameter");
      }
    } else {
      error(peek(), "expect parameter type before parameter name.");
    }
  }

  consume(TKind::RIGHT_PAREN, "expect ')' after parameter");
  consume(TKind::LEFT_BRACE, "expect '{' before function body");
  Stmt::Ptr stmt = blockStmt();

  optional<TypeNode::Ptr> returnType;
  if (isDynamic)
    returnType = nullopt;
  else
    returnType = typeNodeConvertor(ty);

  return make_shared<FuncDecl>(t, name.text, params, returnType, stmt, modi);
}

Ptr Parser::implDecl() {
  AModifier modi = AModifier::DEFAULT;
  Token t=peek();
  if (isAccessModifier()) 
    modi = AModifierConvertor(advance());

  advance();//impl 처리

  Token target =
      consume(TKind::IDENTIFIER, "expect implement target name after 'impl'");
  vector<string> traits;
  if (check(TKind::COLON)) {
    advance();//:처리
    while (!check(TKind::LEFT_BRACE) && !isAtEnd()) {
      traits.push_back(advance().text);
    }
  }
  consume(TKind::LEFT_BRACE, "expect '{' before impl body");
  vector<shared_ptr<FuncDecl>> methods;

  while (!check(TKind::RIGHT_BRACE) && !isAtEnd()) {
    if (isType() && isFunc()) {
      methods.push_back(dynamic_pointer_cast<FuncDecl>(functionDecl()));
    } else
      error(peek(), "only function declare in impl body");
  }

  consume(TKind::RIGHT_BRACE, "expect '}' after impl body");

  return make_shared<ImplDecl>(t, target.text, traits, methods, modi);
}

Ptr Parser::traitDecl() {
  Token t = peek();
  AModifier modi = AModifier::DEFAULT;
  
  if (isAccessModifier())
    modi = AModifierConvertor(advance());

  advance(); // trait 처리

  Token name = consume(TKind::IDENTIFIER, "expect trait name after 'trait'");
  consume(TKind::LEFT_BRACE, "expect '{' before trait body");

  vector<shared_ptr<TraitDecl::TraitSig>> traitSigs;

  while (!check(TKind::RIGHT_BRACE) && !isAtEnd()) {
    Token t = peek();
    if (isType() && isFunc()) {
      if (isAccessModifier())
        error(peek(), "access modifier cannot place in function signiture");
      Token ty = advance();
      Token name =
          consume(TKind::IDENTIFIER, "expect method name after method type");
      consume(TKind::LEFT_PAREN, "expect '(' after method name");
      vector<shared_ptr<FuncDecl::Param>> params;
      while (!check(TKind::RIGHT_PAREN) && !isAtEnd()) {
        if (isType()) {
          Token type = advance();
          Token name = consume(TKind::IDENTIFIER,
                               "expect parameter name after parameter type");
          optional<Expr::Ptr> init;
          if (check(TKind::EQUAL)) {
            advance();//=처리
            init = expression();
          }
          TypeNode::Ptr returnType = typeNodeConvertor(type);
          params.push_back(
              make_shared<FuncDecl::Param>(name.text, returnType, init));
          if (check(TKind::COMMA)) {
            if (!check(TKind::RIGHT_BRACE, 1))
              advance();//,처리
            else
              error(following(), "after ',' need more parameter");
          }
        } else {
          error(peek(), "expect parameter type before parameter name.");
        }
      }
      consume(TKind::SEMICOLON, "expect ';' after method declare");
      TypeNode::Ptr returnType = typeNodeConvertor(ty);
      traitSigs.push_back(
          make_shared<TraitDecl::TraitSig>(t, returnType, name.text, params));

    } else
      error(peek(), "expect function interface struct");
  }

  consume(TKind::RIGHT_BRACE, "expect '}' after parameter");
  return make_shared<TraitDecl>(t, name.text, traitSigs, modi);
}

Ptr Parser::enumDecl() {
  Token t = peek();
  AModifier modi = AModifier::DEFAULT;
  if (isAccessModifier())
    modi = AModifierConvertor(advance());
  advance(); // enum 처리
  Token name = consume(TKind::IDENTIFIER, "expect enum name after 'enum'");
  optional<string> baseEnum = nullopt;

  if (check(TKind::COLON)) {
    advance();//:처리
    baseEnum = consume(TKind::IDENTIFIER, "only enum type inherentale").text;
  }

  consume(TKind::LEFT_BRACE, "exepct '{' before enum body");
  vector<shared_ptr<EnumDecl::Variant>> variants;
  while (!check(TKind::RIGHT_BRACE) && !isAtEnd()) {
    Token name = consume(TKind::IDENTIFIER, "expect name in enum body");
    vector<TypeNode::Ptr> payloads;
    if (check(TKind::LEFT_PAREN)) {
      advance();//(처리
      while (!check(TKind::RIGHT_PAREN) && !isAtEnd()) {
        if (isType()) {
          Token ty = advance();
          payloads.push_back(typeNodeConvertor(ty));
          if (check(TKind::COMMA)) {
            if (!check(TKind::RIGHT_BRACE, 1))
              advance();//,처리
            else
              error(following(), "after ',' need more parameter");
          }
        } else
          error(peek(), "only type and obect place here");
      }
      variants.push_back(make_shared<EnumDecl::Variant>(name.text, payloads));
    }
  }

  return make_shared<EnumDecl>(t, name.text, variants, baseEnum, modi);
}