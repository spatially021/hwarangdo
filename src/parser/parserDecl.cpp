#include "AST/ASTNode.h"
#include "AST/Decl.h"
#include "Parser.h"
#include "Token.h"
#include "util/Error.h"
#include <cassert>
#include <memory>
#include <optional>
#include <string>

using Ptr = Decl::Ptr;

Ptr Parser::classDecl(DeclPrefix prefix) {

  if (contexts.back() != CLASSBODY && contexts.back() != TOPLEVEL)
    Error::diagnostic(prefix.startToken,
                      "class delaration can only declare in top-level "
                      "or other class's block");
  ContextGuard _{contexts, CLASSBODY};

  if (prefix.isConst)
    Error::diagnostic(previous(), "const can place only variation declaration");

  AModifier modi = prefix.modi;
  Token t = prefix.startToken;
  advance(); // class 처리

  Token name = consume(TKind::IDENTIFIER, "expect class name after 'class'.");
  optional<string> base;

  if (check(TKind::EXTENDS)) {
    advance(); // extends 처리
    base = advance().text;
  }

  vector<string> traits;
  if (check(TKind::COLON)) {
    advance(); //: 처리
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

Ptr Parser::structDecl(DeclPrefix prefix) {

  if (contexts.back() != CLASSBODY && contexts.back() != TOPLEVEL)
    Error::diagnostic(prefix.startToken,
                      "struct delaration can only declare in top-level "
                      "or other class's block");
  ContextGuard _{contexts, BLOCK};

  if (prefix.isConst)
    Error::diagnostic(previous(), "const can place only variation declaration");
  AModifier modi = prefix.modi;
  Token t = prefix.startToken;
  advance(); // struct 처리

  Token name = consume(TKind::IDENTIFIER, "expect struct name after 'struct'.");

  consume(TKind::LEFT_BRACE, "expect '{' before struct body");
  vector<shared_ptr<VarDecl>> fields;
  while (!check(TKind::RIGHT_BRACE) && !isAtEnd()) {

    DeclPrefix p = {};
    p.startToken = peek();

    if (isAccessModifier())
      p.modi = AModifierConvertor(advance());
    if (check(TKind::CONST)) {
      p.isConst = true;
      advance();
    }

    if (isType() && !isFunc()) {
      auto var = dynamic_pointer_cast<VarDecl>(varDecl(p));
      if (var) {
        fields.push_back(var);
      } else
        Error::diagnostic(peek(), "this expression is not allowed");
    } else
      Error::diagnostic(peek(), "only var or instance declare here");
  }
  consume(TKind::RIGHT_BRACE, "expect '}' after struct body");

  return make_shared<StructDecl>(t, name.text, fields, modi);
}

Ptr Parser::varDecl(DeclPrefix prefix) {

  if (contexts.back() == TOPLEVEL)
    Error::diagnostic(prefix.startToken,
                      "variation declaration cannot place in top-level");

  AModifier modi = prefix.modi;
  Token t = prefix.startToken;

  Token ty = advance(); // 자료형/객체인스턴스 처리
  Token size = {};
  if (check(TKind::COLON)) {

    if (ty.kind == TKind::IDENTIFIER)
      Error::diagnostic(peek(), "':' is only allowed built-in types");
    advance(); //: 처리
    Token temp = consume(TKind::SIZE, "expect size value");
    assert(temp.text != "");
    switch (ty.kind) {
    case TKind::INT:
      if (!(temp.text[0] == 'i' || temp.text[0] == 'u'))
        Error::diagnostic(temp, "unmatch bitwidth type");
      break;
    case TKind::FLOAT:
      if (temp.text[0] != 'f')
        Error::diagnostic(temp, "unmatch bitwidth type");
      break;
    case TKind::FIXED:
      break;
    case TKind::CHAR:
    case TKind::STRING:
      if (temp.text[0] != 'c')
        Error::diagnostic(temp, "unmatch bitwidth type");
      break;
    default:
      Error::diagnostic(ty, "unexpected type");
    }
  }

  Token name = consume(TKind::IDENTIFIER, "expect var name after type-keyword");

  if (check(TKind::LEFT_BRACKET)) {
    advance(); //[처리
    Expr::Ptr s = expression();
    consume(TKind::RIGHT_BRACKET, "expect ']' after array's size expression");

    Expr::Ptr init = nullptr;

    if (check(TKind::EQUAL)) {
      advance(); //=처리
      init = expression();
    }

    consume(TKind::SEMICOLON, "expect ';' after expression");

    TypeNode::Ptr node = typeNodeConvertor(ty, size);
    auto aNode = make_shared<ArrayTypeNode>(t, node, s);

    return make_shared<ArrayDecl>(t, name.text, aNode, init, !prefix.isConst,
                                  modi);
  }

  Expr::Ptr init = nullptr;

  if (check(TKind::EQUAL)) {
    advance(); //=처리
    init = expression();
  }

  consume(TKind::SEMICOLON, "expect ';' after expression.");
  TypeNode::Ptr node = typeNodeConvertor(ty, size);
  return make_shared<VarDecl>(t, name.text, node, init, !prefix.isConst, modi);
}

Ptr Parser::functionDecl(DeclPrefix prefix, bool isDynamic) {
  if (contexts.back() == TOPLEVEL)
    Error::diagnostic(prefix.startToken,
                      "function declaration cannot place in top-level");
  if (contexts.back() == BLOCK)
    Error::diagnostic(prefix.startToken,
                      "function delcaration cannot place in block");

  ContextGuard _{contexts, BLOCK};

  if (prefix.isConst)
    Error::diagnostic(previous(), "const can place only variation declaration");
  AModifier modi = prefix.modi;
  Token t = prefix.startToken;
  Token ty = advance(); // 자료형/객체/func/void처리

  Token name = consume(TKind::IDENTIFIER, "expect function's name");

  consume(TKind::LEFT_PAREN, "expect '(' after function name");

  vector<shared_ptr<Param>> params;

  while (!check(TKind::RIGHT_PAREN) && !isAtEnd()) {
    bool isBorrow = false;
    if (check(TKind::TILDE)) {
      isBorrow = true;
      advance(); //~처리
    }
    if (isType()) {
      Token type = advance();
      Token n = consume(TKind::IDENTIFIER,
                        "expect parameter name after parameter type");
      optional<Expr::Ptr> init;
      if (check(TKind::EQUAL)) {
        advance(); //=처리
        init = expression();
      }
      TypeNode::Ptr returnType = typeNodeConvertor(type);
      params.push_back(make_shared<Param>(n.text, returnType, init, isBorrow));
      if (check(TKind::COMMA)) {
        if (!check(TKind::RIGHT_PAREN, 1))
          advance(); //,처리
        else
          Error::diagnostic(following(), "after ',' need more parameter");
      }
    } else {
      Error::diagnostic(peek(), "expect parameter type before parameter name.");
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

Ptr Parser::implDecl(DeclPrefix prefix) {
  if (contexts.back() != CLASSBODY && contexts.back() != TOPLEVEL)
    Error::diagnostic(prefix.startToken,
                      "class delaration can only declare in top-level "
                      "or other class's block");

  if (prefix.isConst)
    Error::diagnostic(previous(), "const can place only variation declaration");
  ContextGuard _{contexts, IMPLBODY};

  AModifier modi = prefix.modi;
  Token t = prefix.startToken;
  advance(); // impl 처리

  Token target =
      consume(TKind::IDENTIFIER, "expect implement target name after 'impl'");
  vector<string> traits;
  if (check(TKind::COLON)) {
    advance(); //: 처리
    while (!check(TKind::LEFT_BRACE) && !isAtEnd()) {
      traits.push_back(advance().text);
    }
  }
  consume(TKind::LEFT_BRACE, "expect '{' before impl body");
  vector<shared_ptr<FuncDecl>> methods;

  while (!check(TKind::RIGHT_BRACE) && !isAtEnd()) {
    DeclPrefix p = {};
    p.startToken = peek();
    if (isAccessModifier())
      p.modi = AModifierConvertor(advance());
    if (check(TKind::CONST))
      Error::diagnostic(peek(), "const can place only variation declaration");

    if (isFunc()) {
      methods.push_back(
          dynamic_pointer_cast<FuncDecl>(functionDecl(p, check(TKind::FUNC))));
    } else
      Error::diagnostic(peek(), "only function declare in impl body");
  }
  consume(TKind::RIGHT_BRACE, "expect '}' after impl body");

  return make_shared<ImplDecl>(t, target.text, traits, methods, modi);
}

Ptr Parser::traitDecl(DeclPrefix prefix) {
  if (prefix.isConst)
    Error::diagnostic(previous(), "const can place only variation declaration");
  AModifier modi = prefix.modi;
  Token t = prefix.startToken;
  advance(); // trait 처리

  Token name = consume(TKind::IDENTIFIER, "expect trait name after 'trait'");
  consume(TKind::LEFT_BRACE, "expect '{' before trait body");

  vector<shared_ptr<TraitSig>> traitSigs;

  while (!check(TKind::RIGHT_BRACE) && !isAtEnd()) {
    Token tok = peek();
    if (isFunc()) {
      if (isAccessModifier())
        Error::diagnostic(peek(),
                          "access modifier cannot place in function signiture");
      Token ty = advance();
      Token sigName =
          consume(TKind::IDENTIFIER, "expect method name after method type");
      consume(TKind::LEFT_PAREN, "expect '(' after method name");
      vector<shared_ptr<Param>> params;
      while (!check(TKind::RIGHT_PAREN) && !isAtEnd()) {
        if (isType()) {
          Token type = advance();
          Token n = consume(TKind::IDENTIFIER,
                            "expect parameter name after parameter type");
          optional<Expr::Ptr> init;
          if (check(TKind::EQUAL)) {
            advance(); //=처리
            init = expression();
          }
          TypeNode::Ptr returnType = typeNodeConvertor(type);
          params.push_back(make_shared<Param>(n.text, returnType, init));
          if (check(TKind::COMMA)) {
            if (!check(TKind::RIGHT_BRACE, 1))
              advance(); //,처리
            else
              Error::diagnostic(following(), "after ',' need more parameter");
          }
        } else {
          Error::diagnostic(peek(),
                            "expect parameter type before parameter name.");
        }
      }
      consume(TKind::RIGHT_PAREN, "expect ')' after parameter");
      consume(TKind::SEMICOLON, "expect ';' after method declare");
      TypeNode::Ptr returnType = typeNodeConvertor(ty);
      traitSigs.push_back(
          make_shared<TraitSig>(tok, returnType, sigName.text, params));
    } else
      Error::diagnostic(peek(), "expect function interface struct");
  }

  consume(TKind::RIGHT_BRACE, "expect '}' after parameter");
  return make_shared<TraitDecl>(t, name.text, traitSigs, modi);
}

Ptr Parser::enumDecl(DeclPrefix prefix) {
  if (prefix.isConst)
    Error::diagnostic(previous(), "const can place only variation declaration");
  AModifier modi = prefix.modi;
  Token t = prefix.startToken;
  advance(); // enum 처리
  Token name = consume(TKind::IDENTIFIER, "expect enum name after 'enum'");
  optional<string> baseEnum = nullopt;

  if (check(TKind::COLON)) {
    advance(); //: 처리
    baseEnum = consume(TKind::IDENTIFIER, "only enum type inherentale").text;
  }

  consume(TKind::LEFT_BRACE, "exepct '{' before enum body");
  vector<shared_ptr<EnumDecl::Variant>> variants;
  while (!check(TKind::RIGHT_BRACE) && !isAtEnd()) {
    Token n = consume(TKind::IDENTIFIER, "expect name in enum body");
    if (check(TKind::LEFT_PAREN)) {
      Token ty;
      advance(); //(처리
      if (isType()) {
        ty = advance();
      } else
        Error::diagnostic(peek(), "only type and object place here");
      consume(TKind::RIGHT_PAREN, "expect ')' after payload");
      variants.push_back(
          make_shared<EnumDecl::Variant>(n, n.text, typeNodeConvertor(ty)));
    } else {
      variants.push_back(make_shared<EnumDecl::Variant>(n, n.text));
    }
    if (check(TKind::COMMA)) {
      advance(); //,처리
    }
  }

  consume(TKind::RIGHT_BRACE, "expect '}' after enum body");

  return make_shared<EnumDecl>(t, name.text, variants, baseEnum, modi);
}