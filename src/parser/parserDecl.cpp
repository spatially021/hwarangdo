#include "hrd/AST/ASTNode.h"
#include "hrd/AST/Decl.h"
#include "hrd/AST/DeclContext.h"
#include "hrd/AST/Stmt.h"
#include "hrd/Parser.h"
#include "hrd/SourceSpan.h"
#include "hrd/Token.h"
#include "hrd/util/Error.h"
#include "hrd/util/diagnostic/Diagnostic.h"
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

  if (contexts.back() != DeclContext::TOPLEVEL) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P009);
    dia.labels = {
        {peek().span, "'class' declaration is not allowed here", true},
    };
    engine.emit(dia);
    throw runtime_error("");
  }

  ContextGuard _{contexts, DeclContext::CLASSBODY};

  notFunc(prefix);
  notVar(prefix);

  AModifier modi = prefix.modi;
  Token t = prefix.startToken;
  advance(); // class 처리

  Token name = consume(TKind::IDENTIFIER, DiagnosticCode::HRD_P041,
                       "expected class name after 'class'");
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

  consume(TKind::LEFT_BRACE, DiagnosticCode::HRD_P043,
          "expected '{' to begin class body");
  vector<shared_ptr<VarDecl>> fields;
  vector<shared_ptr<FuncDecl>> methods;
  vector<shared_ptr<Decl>> innterDecl;
  while (!check(TKind::RIGHT_BRACE) && !isAtEnd()) {
    auto b = declaration(DeclContext::CLASSBODY);
    if (!b) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P011);
      dia.labels = {
          {peek().span, "only field and method declarations are allowed here",
           true},
      };
      engine.emit(dia);
      throw runtime_error("");
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

  consume(TKind::RIGHT_BRACE, DiagnosticCode::HRD_P040,
          "expected '}' after class body");
  Token end = previous(); // '}' 토큰
  return make_shared<ClassDecl>(makeSpan(t.span, end.span), name.text, fields,
                                methods, innterDecl, base, traits, modi);
}

Ptr Parser::structDecl(DeclPrefix prefix) {

  if (contexts.back() != DeclContext::TOPLEVEL) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P009);
    dia.labels = {
        {peek().span, "'struct' declaration is not allowed here", true},
    };
    engine.emit(dia);
    throw runtime_error("");
  }
  ContextGuard _{contexts, DeclContext::CLASSBODY};

  notFunc(prefix);
  notVar(prefix);

  AModifier modi = prefix.modi;
  Token t = prefix.startToken;
  advance(); // struct 처리

  Token name = consume(TKind::IDENTIFIER, DiagnosticCode::HRD_P041,
                       "expected struct name after 'struct'");

  consume(TKind::LEFT_BRACE, DiagnosticCode::HRD_P043,
          "expected '{' to begin struct body");
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
      } else {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P010);
        dia.labels = {
            {peek().span, "only field and init declarations are allowed here",
             true},
        };
        engine.emit(dia);
        throw runtime_error("");
      }
    } else if (isInit()) {
      auto init = dynamic_pointer_cast<InitDecl>(initDecl(p));
      if (init == nullptr) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P010);
        dia.labels = {
            {peek().span, "only field and init declarations are allowed here",
             true},
        };
        engine.emit(dia);
        throw runtime_error("");
      }
      inits.push_back(init);
    } else {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P010);
      dia.labels = {
          {peek().span, "only field and init declarations are allowed here",
           true},
      };
      engine.emit(dia);
      throw runtime_error("");
    }
  }
  consume(TKind::RIGHT_BRACE, DiagnosticCode::HRD_P040,
          "expected '}' after struct body");
  auto end = previous();
  return make_shared<StructDecl>(makeSpan(t.span, end.span), name.text, fields,
                                 inits, modi);
}

Ptr Parser::varDecl(DeclPrefix prefix) {

  if (contexts.back() == DeclContext::TOPLEVEL) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P007);
    dia.labels = {
        {peek().span, "expected a declaration here", true},
    };
    engine.emit(dia);
    throw runtime_error("");
  }

  notFunc(prefix);

  AModifier modi = prefix.modi;
  Token t = prefix.startToken;

  TypeNode::Ptr type = parseType();
  Token name = consume(TKind::IDENTIFIER, DiagnosticCode::HRD_P045,
                       "expected variable name after type");
  Expr::Ptr init = nullptr;

  if (check(TKind::EQUAL)) {

    advance(); //=처리
    init = expression();
    if (init == nullptr) {
      Error::internal(t, "variable declaration initializer is null");
    }
  }

  consume(TKind::SEMICOLON, DiagnosticCode::HRD_P044,
          "expected ';' after variable declaration");
  auto end = previous();
  return make_shared<VarDecl>(makeSpan(t.span, end.span), name.text, type,
                              contexts.back(), init, !prefix.isConst,
                              prefix.isRoot, modi);
}

Ptr Parser::functionDecl(DeclPrefix prefix, bool isDynamic) {
  if (contexts.back() == DeclContext::TOPLEVEL) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P007);
    dia.labels = {
        {peek().span, "expected a declaration here", true},
    };
    engine.emit(dia);
    throw runtime_error("");
  }

  if (contexts.back() == DeclContext::BLOCK) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P012);
    dia.labels = {
        {peek().span, "method declaration is not allowed here", true},
    };
    engine.emit(dia);
    throw runtime_error("");
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
  Token name = consume(TKind::IDENTIFIER, DiagnosticCode::HRD_P042,
                       "expected method name here");
  consume(TKind::LEFT_PAREN, DiagnosticCode::HRD_P046,
          "expected '(' after method name");

  vector<shared_ptr<Param>> params;

  while (!check(TKind::RIGHT_PAREN) && !isAtEnd()) {
    if (isType()) {
      TypeNode::Ptr type = parseType();
      Token n = consume(TKind::IDENTIFIER, DiagnosticCode::HRD_P045,
                        "expected parameter name after type");
      ;
      optional<Expr::Ptr> init;
      if (check(TKind::EQUAL)) {
        advance(); //=처리
        init = expression();
      }
      params.push_back(make_shared<Param>(n.text, type, init));
      if (check(TKind::COMMA)) {
        if (!check(TKind::RIGHT_PAREN, 1))
          advance(); //,처리
        else {
          auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P013);
          dia.labels = {
              {peek().span, "expected parameter after ','", true},
          };
          engine.emit(dia);
          throw runtime_error("");
        }
      }
    } else {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P014);
      dia.labels = {
          {peek().span, "parameter type is missing", true},
      };
      engine.emit(dia);
      throw runtime_error("");
    }
  }

  consume(TKind::RIGHT_PAREN, DiagnosticCode::HRD_P047,
          "expected ')' to close parameter list");
  consume(TKind::LEFT_BRACE, DiagnosticCode::HRD_P043,
          "expected '{' to begin method body");
  Stmt::Ptr stmt = blockStmt();

  TypeNode::Ptr returnType;
  returnType = ty;

  auto end = previous();

  return make_shared<FuncDecl>(makeSpan(t.span, end.span), name.text, params,
                               returnType, stmt, modi, prefix.isExtern,
                               prefix.isFrame, prefix.isOverride);
}

Ptr Parser::implDecl(DeclPrefix prefix) {
  if (contexts.back() != DeclContext::TOPLEVEL) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P007);
    dia.labels = {
        {peek().span, "expected a declaration here", true},
    };
    engine.emit(dia);
    throw runtime_error("");
  }

  notFunc(prefix);
  notVar(prefix);
  ContextGuard _{contexts, DeclContext::IMPLBODY};

  AModifier modi = prefix.modi;
  Token t = prefix.startToken;
  advance(); // impl 처리

  Token target = consume(TKind::IDENTIFIER, DiagnosticCode::HRD_P041,
                         "expected type name after 'impl'");
  vector<string> traits;
  if (check(TKind::COLON)) {
    advance(); //: 처리
    while (!check(TKind::LEFT_BRACE) && !isAtEnd()) {
      traits.push_back(advance().text);
    }
  }
  consume(TKind::LEFT_BRACE, DiagnosticCode::HRD_P043,
          "expected '{' to begin impl body");
  vector<shared_ptr<FuncDecl>> methods;

  while (!check(TKind::RIGHT_BRACE) && !isAtEnd()) {
    DeclPrefix p = {};
    p.startToken = peek();
    if (isAccessModifier())
      p.modi = AModifierConvertor(advance());
    if (check(TKind::CONST)) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P015);
      dia.labels = {
          {peek().span, "'const' is not allowed on this declaration", true},
      };
      engine.emit(dia);
      throw runtime_error("");
    }

    if (check(TKind::ROOT)) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P016);
      dia.labels = {
          {peek().span, "'root' is not allowed on this declaration", true},
      };
      engine.emit(dia);
      throw runtime_error("");
    }

    if (isFunc()) {
      methods.push_back(dynamic_pointer_cast<FuncDecl>(functionDecl(p)));
    } else {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P017);
      dia.labels = {
          {peek().span, "field declarations are not allowed in impl blocks",
           true},
      };
      engine.emit(dia);
      throw runtime_error("");
    }
  }
  consume(TKind::RIGHT_BRACE, DiagnosticCode::HRD_P040,
          "expected '}' after impl body");
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

  Token name = consume(TKind::IDENTIFIER, DiagnosticCode::HRD_P041,
                       "expected trait name after 'trait'");

  consume(TKind::LEFT_BRACE, DiagnosticCode::HRD_P043,
          "expected '{' to begin trait body");

  vector<shared_ptr<TraitSig>> traitSigs;

  while (!check(TKind::RIGHT_BRACE) && !isAtEnd()) {
    Token tok = peek();
    if (isFunc()) {
      if (isAccessModifier()) {
        string modifier = peek().text;

        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P018);
        dia.labels = {
            {peek().span,
             "'" + modifier + "' is not allowed on trait method signatures",
             true},
        };
        engine.emit(dia);
        throw runtime_error("");
      }
      Token ty = advance();
      // if (ty.kind == TKind::FUNC) {
      //   auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P019);
      //   dia.labels = {
      //       {peek().span, "trait methods must not use the 'func' keyword",
      //        true},
      //   };
      //   engine.emit(dia);
      //   throw runtime_error("");
      //   // Error::diagnostic(ty, "'func' is not allowed in trait
      //   declarations");
      // }
      Token sigName = consume(TKind::IDENTIFIER, DiagnosticCode::HRD_P042,
                              "expected method name here");

      consume(TKind::LEFT_PAREN, DiagnosticCode::HRD_P046,
              "expected '(' after method name");
      vector<shared_ptr<Param>> params;
      while (!check(TKind::RIGHT_PAREN) && !isAtEnd()) {
        if (isType()) {
          Token type = advance();
          Token n = consume(TKind::IDENTIFIER, DiagnosticCode::HRD_P045,
                            "expected parameter name after type");
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
            else {
              auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P013);
              dia.labels = {
                  {peek().span, "expected parameter after ','", true},
              };
              engine.emit(dia);
              throw runtime_error("");
            }
          }
        } else {
          auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P014);
          dia.labels = {
              {peek().span, "parameter type is missing", true},
          };
          engine.emit(dia);
          throw runtime_error("");
        }
      }

      consume(TKind::RIGHT_PAREN, DiagnosticCode::HRD_P047,
              "expected ')' to close parameter list");
      consume(TKind::SEMICOLON, DiagnosticCode::HRD_P044,
              "expected ';' after method declaration");
      auto e = previous();
      TypeNode::Ptr returnType = typeNodeConvertor(ty);
      traitSigs.push_back(make_shared<TraitSig>(
          makeSpan(ty.span, e.span), returnType, sigName.text, params));
    } else {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P019);
      dia.labels = {
          {peek().span, "only method signatures are allowed here", true},
      };
      engine.emit(dia);
      throw runtime_error("");
    }
  }

  consume(TKind::RIGHT_BRACE, DiagnosticCode::HRD_P040,
          "expected '}' after trait body");
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
  Token name = consume(TKind::IDENTIFIER, DiagnosticCode::HRD_P041,
                       "expected enum name after 'enum'");

  consume(TKind::LEFT_BRACE, DiagnosticCode::HRD_P043,
          "expected '{' to begin enum body");
  vector<shared_ptr<EnumDecl::Variant>> variants;
  while (!check(TKind::RIGHT_BRACE) && !isAtEnd()) {
    Token n = consume(TKind::IDENTIFIER, DiagnosticCode::HRD_P048,
                      "expected enum variant name here");
    if (check(TKind::LEFT_PAREN)) {
      Token ty;
      advance(); //(처리
      if (isType()) {
        ty = advance();
      } else {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P020);
        dia.labels = {
            {peek().span, "expected payload type name", true},
        };
        engine.emit(dia);
        throw runtime_error("");
      }

      consume(TKind::RIGHT_PAREN, DiagnosticCode::HRD_P047,
              "expected ')' to close enum variant payload");
      variants.push_back(
          make_shared<EnumDecl::Variant>(n, n.text, typeNodeConvertor(ty)));
    } else {
      variants.push_back(make_shared<EnumDecl::Variant>(n, n.text));
    }
    if (check(TKind::COMMA)) {
      advance(); //,처리
    }
  }

  consume(TKind::RIGHT_BRACE, DiagnosticCode::HRD_P040,
          "expected '}' after enum body");
  auto end = previous();
  return make_shared<EnumDecl>(makeSpan(t, end), name.text, variants, modi);
}

Ptr Parser::handleDecl(DeclPrefix prefix) {
  Token t = prefix.startToken;
  notFunc(prefix);
  advance(); // Handle 처리
  consume(TKind::LESS, DiagnosticCode::HRD_P049, "expected '<' after 'Handle'");
  TypeNode::Ptr inner;
  if (isType()) {
    inner = parseType();
  } else {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P021);
    dia.labels = {
        {peek().span, "expected type name for Handle", true},
    };
    engine.emit(dia);
    throw runtime_error("");
  }
  consume(TKind::GREATER, DiagnosticCode::HRD_P050,
          "expected '>' after type name");
  auto end = previous();
  Token name = consume(TKind::IDENTIFIER, DiagnosticCode::HRD_P045,
                       "expected handle name after Handle<T>");
  vector<TypeNode::Ptr> tys;
  tys.push_back(inner);
  TypeNode::Ptr type = make_shared<GenericTypeNode>(t, t.text, tys);

  Expr::Ptr init = nullptr;
  if (check(TKind::EQUAL)) {
    advance(); //=처리
    init = expression();
  }
  consume(TKind::SEMICOLON, DiagnosticCode::HRD_P044,
          "expected ';' after handle declaration");
  auto e = previous();
  return make_shared<VarDecl>(makeSpan(t, e), name.text, type, contexts.back(),
                              init, !prefix.isConst, prefix.isRoot,
                              prefix.modi);
}

Ptr Parser::initDecl(DeclPrefix prefix) {
  Token t = prefix.startToken;

  notVar(prefix);

  advance(); // init 처리
  consume(TKind::LEFT_PAREN, DiagnosticCode::HRD_P046,
          "expected '(' after init");
  vector<shared_ptr<Param>> params;

  while (!check(TKind::RIGHT_PAREN) && !isAtEnd()) {
    if (isType()) {
      TypeNode::Ptr type = parseType();
      Token n = consume(TKind::IDENTIFIER, DiagnosticCode::HRD_P045,
                        "expected parameter name after type");
      ;
      optional<Expr::Ptr> init;
      if (check(TKind::EQUAL)) {
        advance(); //=처리
        init = expression();
      }
      params.push_back(make_shared<Param>(n.text, type, init));
      if (check(TKind::COMMA)) {
        if (!check(TKind::RIGHT_PAREN, 1))
          advance(); //,처리
        else {
          auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P013);
          dia.labels = {
              {peek().span, "expected parameter after ','", true},
          };
          engine.emit(dia);
          throw runtime_error("");
        }
      }
    } else {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P014);
      dia.labels = {
          {peek().span, "parameter type is missing", true},
      };
      engine.emit(dia);
      throw runtime_error("");
    }
  }

  consume(TKind::RIGHT_PAREN, DiagnosticCode::HRD_P047,
          "expected ')' to close parameter list");

  if (check(TKind::SEMICOLON)) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P022);
    dia.labels = {
        {peek().span, "init methods cannot be called here", true},
    };
    engine.emit(dia);
    throw runtime_error("");
  }
  if (contexts.back() == DeclContext::TOPLEVEL) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P007);
    dia.labels = {
        {peek().span, "expected a init declaration here", true},
    };
    engine.emit(dia);
    throw runtime_error("");
  }

  if (contexts.back() == DeclContext::BLOCK) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P012);
    dia.labels = {
        {peek().span, "init method declaration is not allowed here", true},
    };
    engine.emit(dia);
    throw runtime_error("");
  }
  if (prefix.modi != AModifier::PUBLIC) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P023);
    dia.labels = {
        {peek().span, "init method must be public", true},
    };
    engine.emit(dia);
    throw runtime_error("");
  }
  consume(TKind::LEFT_BRACE, DiagnosticCode::HRD_P043,
          "expected '{' to begin init method body");
  ContextGuard _(contexts, DeclContext::BLOCK);
  Stmt::Ptr stmt = blockStmt();
  auto end = previous();
  return make_shared<InitDecl>(makeSpan(t, end), params, stmt,
                               prefix.isOverride);
}

Ptr Parser::onDestroyDecl(DeclPrefix prefix) {
  Token t = prefix.startToken;

  notVar(prefix);

  advance(); // onDestroy 처리
  consume(TKind::LEFT_PAREN, DiagnosticCode::HRD_P046,
          "expected '(' after onDestroy");

  consume(TKind::RIGHT_PAREN, DiagnosticCode::HRD_P047,
          "expected ')' to close parameter list");

  if (check(TKind::SEMICOLON)) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P024);
    dia.labels = {
        {peek().span, "onDestroy is called automatically during destruction",
         true},
    };
    engine.emit(dia);
    throw runtime_error("");
  }
  if (contexts.back() == DeclContext::TOPLEVEL) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P007);
    dia.labels = {
        {peek().span, "expected a onDestroy declaration here", true},
    };
    engine.emit(dia);
    throw runtime_error("");
  }

  if (contexts.back() == DeclContext::BLOCK) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P012);
    dia.labels = {
        {peek().span, "onDestroy method declaration is not allowed here", true},
    };
    engine.emit(dia);
    throw runtime_error("");
  }

  // if (contexts.back() == DeclContext::TOPLEVEL){

  // }
  //   // Error::diagnostic(prefix.startToken,
  //   //                   "onDestroy declarations are not allowed at top
  //   level");

  // if (contexts.back() == DeclContext::BLOCK)
  //   Error::diagnostic(prefix.startToken,
  //                     "onDestroy declarations are not allowed in block
  //                     scopes");

  if (prefix.modi != AModifier::PUBLIC) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P025);
    dia.labels = {
        {peek().span, "onDestroy method must be public", true},
    };
    engine.emit(dia);
    throw runtime_error("");
  }
  consume(TKind::LEFT_BRACE, DiagnosticCode::HRD_P043,
          "expected '{' to begin onDestroy method body");
  ContextGuard _(contexts, DeclContext::BLOCK);
  Stmt::Ptr stmt = blockStmt();
  auto end = previous();
  return make_shared<OnDestroyDecl>(makeSpan(t, end), stmt);
}