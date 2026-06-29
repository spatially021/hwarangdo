#include "hrd/AST/ASTNode.h"
#include "hrd/AST/Decl.h"
#include "hrd/AST/DeclContext.h"
#include "hrd/AST/Stmt.h"
#include "hrd/Parser.h"
#include "hrd/SourceSpan.h"
#include "hrd/Token.h"
#include "hrd/util/Error.h"
#include <cassert>
#include <memory>
#include <optional>
#include <string>
// TODO(parser): Refactor declaration parsing.
// Current declaration parsing is patched around class/struct/impl-specific
// cases. Special members such as init exposed duplicated and inconsistent
// handling. Unify member declaration parsing around
// field/init/method/type-member classification.
using Ptr = Decl::Ptr;

Ptr Parser::classDecl(DeclPrefix prefix) {

  if (contexts.back() != DeclContext::CLASSBODY &&
      contexts.back() != DeclContext::TOPLEVEL)
    Error::diagnostic(prefix.startToken, "class declarations are only allowed "
                                         "at top level or inside class bodies");
  ContextGuard _{contexts, DeclContext::CLASSBODY};

  notFunc(prefix);
  notVar(prefix);

  AModifier modi = prefix.modi;
  Token t = prefix.startToken;
  advance(); // class 처리

  Token name = consume(TKind::IDENTIFIER, "expected class name after 'class'");
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

  consume(TKind::LEFT_BRACE, "expected '{' before class body");
  vector<shared_ptr<VarDecl>> fields;
  vector<shared_ptr<FuncDecl>> methods;
  vector<shared_ptr<Decl>> innterDecl;
  while (!check(TKind::RIGHT_BRACE) && !isAtEnd()) {
    auto b = declaration(DeclContext::CLASSBODY);
    if (!b) {
      Error::diagnostic(b->span,
                        "only declarations are allowed in class bodies");
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

  consume(TKind::RIGHT_BRACE, "expected '}' after class body");
  Token end = previous(); // '}' 토큰
  return make_shared<ClassDecl>(makeSpan(t.span, end.span), name.text, fields,
                                methods, innterDecl, base, traits, modi);
}

Ptr Parser::structDecl(DeclPrefix prefix) {

  if (contexts.back() != DeclContext::CLASSBODY &&
      contexts.back() != DeclContext::TOPLEVEL)
    Error::diagnostic(prefix.startToken, "struct declarations are only allowed "
                                         "at top level or inside class bodies");
  ContextGuard _{contexts, DeclContext::CLASSBODY};

  notFunc(prefix);
  notVar(prefix);

  AModifier modi = prefix.modi;
  Token t = prefix.startToken;
  advance(); // struct 처리

  Token name =
      consume(TKind::IDENTIFIER, "expected struct name after 'struct'");
  consume(TKind::LEFT_BRACE, "expected '{' before struct body");
  vector<shared_ptr<VarDecl>> fields;
  vector<shared_ptr<InitDecl>> inits;
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
        Error::diagnostic(
            peek(),
            "only field and init declarations are allowed in struct bodies");
    } else if (isInit()) {
      auto init = dynamic_pointer_cast<InitDecl>(initDecl(p));
      if (init == nullptr) {
        Error::diagnostic(
            peek(),
            "only field and init declarations are allowed in struct bodies");
      }
      inits.push_back(init);
    } else
      Error::diagnostic(
          peek(),
          "only field and init declarations are allowed in struct bodies");
  }
  consume(TKind::RIGHT_BRACE, "expected '}' after struct body");
  auto end = previous();
  return make_shared<StructDecl>(makeSpan(t.span, end.span), name.text, fields,
                                 inits, modi);
}

Ptr Parser::varDecl(DeclPrefix prefix) {

  if (contexts.back() == DeclContext::TOPLEVEL) {
    Error::diagnostic(prefix.startToken,
                      "variable declarations are not allowed at top level");
  }

  notFunc(prefix);

  AModifier modi = prefix.modi;
  Token t = prefix.startToken;

  TypeNode::Ptr type = parseType();
  Token name =
      consume(TKind::IDENTIFIER, "expected variable name after type specifier");
  Expr::Ptr init = nullptr;

  if (check(TKind::EQUAL)) {

    advance(); //=처리
    init = expression();
    if (init == nullptr) {
      Error::internal(t, "variable declaration initializer is null");
    }
  }

  consume(TKind::SEMICOLON, "expected ';' after variable declaration");
  auto end = previous();
  return make_shared<VarDecl>(makeSpan(t.span, end.span), name.text, type,
                              contexts.back(), init, !prefix.isConst,
                              prefix.isRoot, modi);
}

Ptr Parser::functionDecl(DeclPrefix prefix, bool isDynamic) {
  if (contexts.back() == DeclContext::TOPLEVEL) {
    Error::diagnostic(prefix.startToken,
                      "function declarations are not allowed at top level");
  }

  if (contexts.back() == DeclContext::BLOCK) {
    Error::diagnostic(prefix.startToken,
                      "function declarations are not allowed in block scopes");
  }

  ContextGuard _{contexts, DeclContext::BLOCK};
  notVar(prefix);
  AModifier modi = prefix.modi;
  Token t = prefix.startToken;

  TypeNode::Ptr ty = nullptr;

  if (!isDynamic) {
    ty = parseType();
  } else {
    advance(); // func 소비
  }
  Token name = consume(TKind::IDENTIFIER, "expected function name");
  consume(TKind::LEFT_PAREN, "expected '(' after function name");

  vector<shared_ptr<Param>> params;

  while (!check(TKind::RIGHT_PAREN) && !isAtEnd()) {
    if (isType()) {
      TypeNode::Ptr type = parseType();
      Token n = consume(TKind::IDENTIFIER,
                        "expected parameter name after parameter type");
      optional<Expr::Ptr> init;
      if (check(TKind::EQUAL)) {
        advance(); //=처리
        init = expression();
      }
      params.push_back(make_shared<Param>(n.text, type, init));
      if (check(TKind::COMMA)) {
        if (!check(TKind::RIGHT_PAREN, 1))
          advance(); //,처리
        else
          Error::diagnostic(following(),
                            "expected parameter declaration after ','");
      }
    } else {
      Error::diagnostic(peek(),
                        "expected parameter type before parameter name");
    }
  }

  consume(TKind::RIGHT_PAREN, "expected ')' after parameter list");
  consume(TKind::LEFT_BRACE, "expected '{' before function body");
  Stmt::Ptr stmt = blockStmt();

  optional<TypeNode::Ptr> returnType;
  if (isDynamic)
    returnType = nullopt;
  else
    returnType = ty;

  auto end = previous();

  return make_shared<FuncDecl>(makeSpan(t.span, end.span), name.text, params,
                               returnType, stmt, modi, prefix.isExtern,
                               prefix.isFrame, prefix.isOverride);
}

Ptr Parser::implDecl(DeclPrefix prefix) {
  if (contexts.back() != DeclContext::CLASSBODY &&
      contexts.back() != DeclContext::TOPLEVEL) {
    Error::diagnostic(prefix.startToken,
                      "impl declarations are only allowed at top level");
  }

  notFunc(prefix);
  notVar(prefix);
  ContextGuard _{contexts, DeclContext::IMPLBODY};

  AModifier modi = prefix.modi;
  Token t = prefix.startToken;
  advance(); // impl 처리

  Token target =
      consume(TKind::IDENTIFIER, "expected target type name after 'impl'");
  vector<string> traits;
  if (check(TKind::COLON)) {
    advance(); //: 처리
    while (!check(TKind::LEFT_BRACE) && !isAtEnd()) {
      traits.push_back(advance().text);
    }
  }
  consume(TKind::LEFT_BRACE, "expected '{' before impl body");
  vector<shared_ptr<FuncDecl>> methods;

  while (!check(TKind::RIGHT_BRACE) && !isAtEnd()) {
    DeclPrefix p = {};
    p.startToken = peek();
    if (isAccessModifier())
      p.modi = AModifierConvertor(advance());
    if (check(TKind::CONST))
      Error::diagnostic(peek(),
                        "'const' is only allowed on variable declarations");

    if (check(TKind::ROOT))
      Error::diagnostic(peek(),
                        "'root' is only allowed on variable declarations");

    if (isFunc()) {
      methods.push_back(
          dynamic_pointer_cast<FuncDecl>(functionDecl(p, check(TKind::FUNC))));
    } else
      Error::diagnostic(
          peek(), "only function declarations are allowed in impl bodies");
  }
  consume(TKind::RIGHT_BRACE, "expected '}' after impl body");
  auto end = previous();
  return make_shared<ImplDecl>(makeSpan(t.span, end.span), target.text, traits,
                               methods, modi);
}

Ptr Parser::traitDecl(DeclPrefix prefix) {
  notFunc(prefix);
  notVar(prefix);
  AModifier modi = prefix.modi;
  Token t = prefix.startToken;
  advance(); // trait 처리

  Token name = consume(TKind::IDENTIFIER, "expected trait name after 'trait'");
  consume(TKind::LEFT_BRACE, "expected '{' before trait body");
  vector<shared_ptr<TraitSig>> traitSigs;

  while (!check(TKind::RIGHT_BRACE) && !isAtEnd()) {
    Token tok = peek();
    if (isFunc()) {
      if (isAccessModifier())
        Error::diagnostic(
            peek(),
            "access modifiers are not allowed in trait method signatures");
      Token ty = advance();
      if (ty.kind == TKind::FUNC) {
        Error::diagnostic(ty, "'func' is not allowed in trait declarations");
      }
      Token sigName =
          consume(TKind::IDENTIFIER, "expected method name after return type");
      consume(TKind::LEFT_PAREN, "expected '(' after method name");
      vector<shared_ptr<Param>> params;
      while (!check(TKind::RIGHT_PAREN) && !isAtEnd()) {
        if (isType()) {
          Token type = advance();
          Token n = consume(TKind::IDENTIFIER,
                            "expected parameter name after parameter type");
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
              Error::diagnostic(following(),
                                "expected parameter declaration after ','");
          }
        } else {
          Error::diagnostic(peek(),
                            "expected parameter type before parameter name");
        }
      }
      consume(TKind::RIGHT_PAREN, "expected ')' after parameter list");
      consume(TKind::SEMICOLON, "expected ';' after method declaration");
      auto e = previous();
      TypeNode::Ptr returnType = typeNodeConvertor(ty);
      traitSigs.push_back(make_shared<TraitSig>(
          makeSpan(ty.span, e.span), returnType, sigName.text, params));
    } else
      Error::diagnostic(peek(),
                        "only method signatures are allowed in trait bodies");
  }

  consume(TKind::RIGHT_BRACE, "expected '}' after trait body");
  auto end = previous();
  return make_shared<TraitDecl>(makeSpan(t.span, end.span), name.text,
                                traitSigs, modi);
}

Ptr Parser::enumDecl(DeclPrefix prefix) {

  notFunc(prefix);
  notVar(prefix);
  AModifier modi = prefix.modi;
  Token t = prefix.startToken;
  advance(); // enum 처리
  Token name = consume(TKind::IDENTIFIER, "expected enum name after 'enum'");

  consume(TKind::LEFT_BRACE, "expected '{' before enum body");
  vector<shared_ptr<EnumDecl::Variant>> variants;
  while (!check(TKind::RIGHT_BRACE) && !isAtEnd()) {
    Token n = consume(TKind::IDENTIFIER, "expected enum variant name");
    if (check(TKind::LEFT_PAREN)) {
      Token ty;
      advance(); //(처리
      if (isType()) {
        ty = advance();
      } else
        Error::diagnostic(peek(), "expected type name in enum variant payload");
      consume(TKind::RIGHT_PAREN, "expected ')' after enum variant payload");
      variants.push_back(
          make_shared<EnumDecl::Variant>(n, n.text, typeNodeConvertor(ty)));
    } else {
      variants.push_back(make_shared<EnumDecl::Variant>(n, n.text));
    }
    if (check(TKind::COMMA)) {
      advance(); //,처리
    }
  }

  consume(TKind::RIGHT_BRACE, "expected '}' after enum body");
  auto end = previous();
  return make_shared<EnumDecl>(makeSpan(t, end), name.text, variants, modi);
}

Ptr Parser::handleDecl(DeclPrefix prefix) {
  Token t = prefix.startToken;
  notFunc(prefix);
  advance(); // Handle 처리
  consume(TKind::LESS, "expected '<' after 'Handle'");
  TypeNode::Ptr inner;
  if (isType()) {
    inner = parseType();
  } else {
    Error::diagnostic(prefix.startToken, "expected type name after '<'");
  }
  consume(TKind::GREATER, "expected '>' after type name");
  auto end = previous();
  Token name =
      consume(TKind::IDENTIFIER, "expected handle name after 'handle'");
  vector<TypeNode::Ptr> tys;
  tys.push_back(inner);
  TypeNode::Ptr type = make_shared<GenericTypeNode>(t, t.text, tys);

  Expr::Ptr init = nullptr;
  if (check(TKind::EQUAL)) {
    advance(); //=처리
    init = expression();
  }
  consume(TKind::SEMICOLON, "expected ';' after handle declaration");
  auto e = previous();
  return make_shared<VarDecl>(makeSpan(t, e), name.text, type, contexts.back(),
                              init, !prefix.isConst, prefix.isRoot,
                              prefix.modi);
}

Ptr Parser::initDecl(DeclPrefix prefix) {
  Token t = prefix.startToken;

  notVar(prefix);

  advance(); // init 처리
  consume(TKind::LEFT_PAREN, "expected '(' after 'init'");
  vector<shared_ptr<Param>> params;

  while (!check(TKind::RIGHT_PAREN) && !isAtEnd()) {
    if (isType()) {
      TypeNode::Ptr type = parseType();
      Token n = consume(TKind::IDENTIFIER,
                        "expected parameter name after parameter type");
      optional<Expr::Ptr> init;
      if (check(TKind::EQUAL)) {
        advance(); //=처리
        init = expression();
      }
      params.push_back(make_shared<Param>(n.text, type, init));
      if (check(TKind::COMMA)) {
        if (!check(TKind::RIGHT_PAREN, 1))
          advance(); //,처리
        else
          Error::diagnostic(following(),
                            "expected parameter declaration after ','");
      }
    } else {
      Error::diagnostic(peek(),
                        "expected parameter type before parameter name");
    }
  }

  consume(TKind::RIGHT_PAREN, "expected ')' after parameter list");

  if (check(TKind::SEMICOLON)) {
    Error::diagnostic(t, "init methods cannot be called directly");
  }
  if (contexts.back() == DeclContext::TOPLEVEL)
    Error::diagnostic(prefix.startToken,
                      "init declarations are not allowed at top level");

  if (contexts.back() == DeclContext::BLOCK)
    Error::diagnostic(prefix.startToken,
                      "init declarations are not allowed in block scopes");
  if (prefix.modi != AModifier::PUBLIC) {
    Error::diagnostic(t, "init methods must be public");
  }
  consume(TKind::LEFT_BRACE, "expected '{' before init body");
  ContextGuard _(contexts, DeclContext::BLOCK);
  Stmt::Ptr stmt = blockStmt();
  auto end = previous();
  return make_shared<InitDecl>(makeSpan(t, end), params, stmt,
                               prefix.isOverride);
}