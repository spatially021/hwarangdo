#include "AST/ASTNode.h"
#include "AST/Decl.h"
#include "AST/Stmt.h"
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

  notFunc(prefix);
  notVar(prefix);

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
  vector<shared_ptr<VarDecl>> fields;
  vector<shared_ptr<FuncDecl>> methods;
  vector<shared_ptr<Decl>> innterDecl;
  while (!check(TKind::RIGHT_BRACE) && !isAtEnd()) {
    auto b = declaration(CLASSBODY);
    if (!b) {
      Error::diagnostic(b->token, "not declare statement : " + b->token.text);
    }
    if (auto f = dynamic_pointer_cast<VarDecl>(b)) {
      fields.push_back(f);
    } else if (auto m = dynamic_pointer_cast<FuncDecl>(b)) {
      methods.push_back(m);
    } else if (auto i = dynamic_pointer_cast<InitDecl>(b)) {
      methods.push_back(i);
    } else {
      innterDecl.push_back(b);
    }
  }

  consume(TKind::RIGHT_BRACE, "expect '}' after class body");

  return make_shared<ClassDecl>(t, name.text, fields, methods, innterDecl, base,
                                traits, modi);
}

Ptr Parser::structDecl(DeclPrefix prefix) {

  if (contexts.back() != CLASSBODY && contexts.back() != TOPLEVEL)
    Error::diagnostic(prefix.startToken,
                      "struct delaration can only declare in top-level "
                      "or other class's block");
  ContextGuard _{contexts, CLASSBODY};

  notFunc(prefix);
  notVar(prefix);
  if (prefix.isFrame) {
    Error::diagnostic(previous(), "frame can place only function declaration");
  }

  if (prefix.isOverride) {
    Error::diagnostic(previous(),
                      "override can place only function declaration");
  }
  if (prefix.isAsync) {
    Error::diagnostic(previous(), "async can place only function declaration");
  }

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

  notFunc(prefix);

  AModifier modi = prefix.modi;
  Token t = prefix.startToken;

  TypeNode::Ptr type = parseType();
  Token name = consume(TKind::IDENTIFIER, "expect var name after type-keyword");

  if (contexts.back() == CLASSBODY) {
  }

  if (check(TKind::LEFT_BRACKET)) {
    advance(); //[처리
    Expr::Ptr s = expression();
    consume(TKind::RIGHT_BRACKET, "expect ']' after array's size expression");

    Expr::Ptr init = nullptr;

    if (check(TKind::EQUAL)) {
      advance(); //=처리
      init = expression();
      if (init == nullptr) {
        Error::internal(t, "var decl init is nullptr");
      }
    }

    consume(TKind::SEMICOLON, "expect ';' after expression");

    auto aNode = make_shared<ArrayTypeNode>(t, type, s);

    return make_shared<ArrayDecl>(t, name.text, aNode, init, !prefix.isConst,
                                  prefix.isRoot, modi);
  }

  Expr::Ptr init = nullptr;

  if (check(TKind::EQUAL)) {
    advance(); //=처리
    init = expression();
    if (init == nullptr) {
      Error::internal(t, "var decl init is nullptr");
    }
  }

  consume(TKind::SEMICOLON, "expect ';' after expression.");
  return make_shared<VarDecl>(t, name.text, type, init, !prefix.isConst,
                              prefix.isRoot, modi);
}

Ptr Parser::functionDecl(DeclPrefix prefix, bool isDynamic) {
  if (contexts.back() == TOPLEVEL)
    Error::diagnostic(prefix.startToken,
                      "function declaration cannot place in top-level");
  if (contexts.back() == BLOCK)
    Error::diagnostic(prefix.startToken,
                      "function delcaration cannot place in block");

  ContextGuard _{contexts, BLOCK};
  notVar(prefix);
  AModifier modi = prefix.modi;
  Token t = prefix.startToken;
  TypeNode::Ptr ty = parseType();
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
      TypeNode::Ptr type = parseType();
      Token n = consume(TKind::IDENTIFIER,
                        "expect parameter name after parameter type");
      optional<Expr::Ptr> init;
      if (check(TKind::EQUAL)) {
        advance(); //=처리
        init = expression();
      }
      params.push_back(make_shared<Param>(n.text, type, init, isBorrow));
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
    returnType = ty;

  return make_shared<FuncDecl>(t, name.text, params, returnType, stmt, modi,
                               prefix.isExtern, prefix.isFrame,
                               prefix.isOverride);
}

Ptr Parser::implDecl(DeclPrefix prefix) {
  if (contexts.back() != CLASSBODY && contexts.back() != TOPLEVEL)
    Error::diagnostic(prefix.startToken,
                      "class delaration can only declare in top-level "
                      "or other class's block");
  notFunc(prefix);
  notVar(prefix);
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
  notFunc(prefix);
  notVar(prefix);
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

  notFunc(prefix);
  notVar(prefix);
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

Ptr Parser::handleDecl(DeclPrefix prefix) {
  Token t = prefix.startToken;
  notFunc(prefix);
  if (prefix.isRoot) {
    Error::diagnostic(prefix.startToken, "root not allowed in handle declare");
  }

  advance(); // Handle 처리
  consume(TKind::LESS, "need '<' after handle");
  TypeNode::Ptr inner;
  if (isType()) {
    inner = parseType();
  } else {
    Error::diagnostic(prefix.startToken, "after < need type");
  }
  consume(TKind::GREATER, "need '>' after type");
  Token name = consume(TKind::IDENTIFIER, "expect handle name after handle");
  vector<TypeNode::Ptr> tys;
  tys.push_back(inner);
  TypeNode::Ptr type = make_shared<GenericTypeNode>(t, t.text, tys);

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

    auto aNode = make_shared<ArrayTypeNode>(t, type, s);

    return make_shared<ArrayDecl>(t, name.text, aNode, init, !prefix.isConst,
                                  prefix.isRoot, prefix.modi);
  }

  Expr::Ptr init = nullptr;
  if (check(TKind::EQUAL)) {
    advance(); //=처리
    init = expression();
  }
  consume(TKind::SEMICOLON, "expect ';' after expression.");
  return make_shared<VarDecl>(t, name.text, type, init, !prefix.isConst,
                              prefix.isRoot, prefix.modi);
}

Ptr Parser::initDecl(DeclPrefix prefix) {
  Token t = prefix.startToken;
  if (contexts.back() == TOPLEVEL)
    Error::diagnostic(prefix.startToken,
                      "function declaration cannot place in top-level");
  if (contexts.back() == BLOCK)
    Error::diagnostic(prefix.startToken,
                      "function delcaration cannot place in block");

  notVar(prefix);
  if (prefix.modi != AModifier::PUBLIC) {
    Error::diagnostic(t, "init method must be public");
  }

  advance(); // init 처리
  consume(TKind::LEFT_PAREN, "expect '(' after init");

  vector<shared_ptr<Param>> params;

  while (!check(TKind::RIGHT_PAREN) && !isAtEnd()) {
    bool isBorrow = false;
    if (check(TKind::TILDE)) {
      isBorrow = true;
      advance(); //~처리
    }
    if (isType()) {
      TypeNode::Ptr type = parseType();
      Token n = consume(TKind::IDENTIFIER,
                        "expect parameter name after parameter type");
      optional<Expr::Ptr> init;
      if (check(TKind::EQUAL)) {
        advance(); //=처리
        init = expression();
      }
      params.push_back(make_shared<Param>(n.text, type, init, isBorrow));
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
  return make_shared<InitDecl>(t, params, stmt, prefix.isOverride);
}