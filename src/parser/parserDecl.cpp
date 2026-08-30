#include "hrd/AST/ASTNode.h"
#include "hrd/AST/Decl.h"
#include "hrd/AST/DeclContext.h"
#include "hrd/AST/Stmt.h"
#include "hrd/Parser.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SourceSpan.h"
#include "hrd/Token.h"
#include "hrd/diagnostic/Diagnostic.h"
#include "hrd/util/Error.h"
#include <cassert>
#include <memory>
#include <optional>
#include <string>
#include <vector>
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
    recover.recover();
  }

  ContextGuard _{contexts, DeclContext::CLASSBODY};

  notFunc(prefix);
  notVar(prefix);

  AModifier modi = prefix.modi;
  Token t = prefix.startToken;
  advance(); // class 처리

  Token name = consume(TKind::IDENTIFIER, DiagnosticCode::HRD_P041,
                       "expected class name after 'class'");
  optional<StringDatum> base;

  if (check(TKind::EXTENDS)) {
    advance(); // extends 처리
    auto b = advance();
    base = StringDatum(b.text, b.span);
  }

  vector<StringDatum> traits;
  if (check(TKind::COLON)) {
    advance(); //: 처리
    do {
      if (check(TKind::COMMA)) {
        advance();
      }
      auto tok = consume(TKind::IDENTIFIER, DiagnosticCode::HRD_P059,
                         "trait name expected here");
      traits.push_back(StringDatum(tok.text, tok.span));
    } while (check(TKind::COMMA) && !isAtEnd());
    if (check(TKind::IDENTIFIER)) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P058);
      dia.labels = {
          {peek().span, "expected ',' before this trait", true},
      };
      dia.helps = {{"insert ',' between the trait names"}};
      engine.emit(dia);
      recover.recover();
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
      recover.recover();
    }
    if (auto f = dynamic_pointer_cast<VarDecl>(b)) {
      fields.push_back(f);
    } else if (auto m = dynamic_pointer_cast<FuncDecl>(b)) {
      methods.push_back(m);
    } else if (auto i = dynamic_pointer_cast<InitDecl>(b)) {
      methods.push_back(i);
    } else {
      Error::internal("unreachable");
    }
  }

  consume(TKind::RIGHT_BRACE, DiagnosticCode::HRD_P040,
          "expected '}' after class body");
  Token end = previous(); // '}' 토큰
  return make_shared<ClassDecl>(makeSpan(t.span, end.span), name.text, fields,
                                methods, base, traits, modi);
}

Ptr Parser::structDecl(DeclPrefix prefix) {

  if (contexts.back() != DeclContext::TOPLEVEL) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P009);
    dia.labels = {
        {peek().span, "'struct' declaration is not allowed here", true},
    };
    engine.emit(dia);
    recover.recover();
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
        recover.recover();
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
        recover.recover();
      }
      inits.push_back(init);
    } else {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P010);
      dia.labels = {
          {peek().span, "only field and init declarations are allowed here",
           true},
      };
      engine.emit(dia);
      recover.recover();
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
    recover.recover();
  }

  notFunc(prefix);

  AModifier modi = prefix.modi;
  Token t = prefix.startToken;

  TypeNode::Ptr type = parseType();
  if (type->onlySize) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P069);
    dia.labels = {{type->span, "type abbreviation used here", true}};
    dia.notes = {{"type abbreviations can only be used with a primitive type "
                  "or in a cast "
                  "expression"}};
    dia.helps = {{"specify the primitive type, such as 'int:i32', or use the "
                  "abbreviation in a cast"}};
  }
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

Ptr Parser::functionDecl(DeclPrefix prefix, bool) {
  if (contexts.back() == DeclContext::TOPLEVEL) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P007);
    dia.labels = {
        {peek().span, "expected a declaration here", true},
    };
    engine.emit(dia);
    recover.recover();
  }

  if (contexts.back() == DeclContext::BLOCK) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P012);
    dia.labels = {
        {peek().span, "method declaration is not allowed here", true},
    };
    engine.emit(dia);
    recover.recover();
  }

  ContextGuard _{contexts, DeclContext::BLOCK};
  notVar(prefix);
  AModifier modi = prefix.modi;
  Token t = prefix.startToken;

  TypeNode::Ptr ty = nullptr;

  ty = parseType();
  if (ty->onlySize) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P069);
    dia.labels = {{ty->span, "type abbreviation used here", true}};
    dia.notes = {{"type abbreviations can only be used with a primitive type "
                  "or in a cast "
                  "expression"}};
    dia.helps = {{"specify the primitive type, such as 'int:i32', or use the "
                  "abbreviation in a cast"}};
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
          recover.recover();
        }
      }
    } else {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P014);
      dia.labels = {
          {peek().span, "parameter type is missing", true},
      };
      engine.emit(dia);
      recover.recover();
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
    recover.recover();
  }

  notFunc(prefix);
  notVar(prefix);
  ContextGuard _{contexts, DeclContext::IMPLBODY};

  AModifier modi = prefix.modi;
  Token t = prefix.startToken;
  advance(); // impl 처리

  Token tok = consume(TKind::IDENTIFIER, DiagnosticCode::HRD_P041,
                      "expected type name after 'impl'");
  StringDatum target = StringDatum(tok.text, tok.span);
  vector<StringDatum> traits;
  unordered_map<TypeSymbol *, SourceSpan> traitSapn;
  if (check(TKind::COLON)) {
    advance(); //: 처리
    do {
      if (check(TKind::COMMA)) {
        advance();
      }
      auto to = consume(TKind::IDENTIFIER, DiagnosticCode::HRD_P059,
                        "trait name expected here");
      traits.push_back(StringDatum(to.text, to.span));
    } while (check(TKind::COMMA) && !isAtEnd());
    if (check(TKind::IDENTIFIER)) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P058);
      dia.labels = {
          {peek().span, "expected ',' before this trait", true},
      };
      dia.helps = {{"insert ',' between the trait names"}};
      engine.emit(dia);
      recover.recover();
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
      recover.recover();
    }

    if (check(TKind::ROOT)) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P016);
      dia.labels = {
          {peek().span, "'root' is not allowed on this declaration", true},
      };
      engine.emit(dia);
      recover.recover();
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
      recover.recover();
    }
  }
  consume(TKind::RIGHT_BRACE, DiagnosticCode::HRD_P040,
          "expected '}' after impl body");
  auto end = previous();
  return make_shared<ImplDecl>(makeSpan(t.span, end.span), target, traits,
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
        recover.recover();
      }
      Token ty = advance();
      // if (ty.kind == TKind::FUNC) {
      //   auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P019);
      //   dia.labels = {
      //       {peek().span, "trait methods must not use the 'func' keyword",
      //        true},
      //   };
      //   engine.emit(dia);
      //   recover.recover();
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
              recover.recover();
            }
          }
        } else {
          auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P014);
          dia.labels = {
              {peek().span, "parameter type is missing", true},
          };
          engine.emit(dia);
          recover.recover();
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
      recover.recover();
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
        recover.recover();
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
    recover.recover();
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
          recover.recover();
        }
      }
    } else {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P014);
      dia.labels = {
          {peek().span, "parameter type is missing", true},
      };
      engine.emit(dia);
      recover.recover();
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
    recover.recover();
  }
  if (contexts.back() == DeclContext::TOPLEVEL) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P007);
    dia.labels = {
        {peek().span, "expected a init declaration here", true},
    };
    engine.emit(dia);
    recover.recover();
  }

  if (contexts.back() == DeclContext::BLOCK) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P012);
    dia.labels = {
        {peek().span, "init method declaration is not allowed here", true},
    };
    engine.emit(dia);
    recover.recover();
  }
  if (prefix.modi != AModifier::PUBLIC) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P023);
    dia.labels = {
        {peek().span, "init method must be public", true},
    };
    engine.emit(dia);
    recover.recover();
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
    recover.recover();
  }
  if (contexts.back() == DeclContext::TOPLEVEL) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P007);
    dia.labels = {
        {peek().span, "expected a onDestroy declaration here", true},
    };
    engine.emit(dia);
    recover.recover();
  }

  if (contexts.back() == DeclContext::BLOCK) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P012);
    dia.labels = {
        {peek().span, "onDestroy method declaration is not allowed here", true},
    };
    engine.emit(dia);
    recover.recover();
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
    recover.recover();
  }
  consume(TKind::LEFT_BRACE, DiagnosticCode::HRD_P043,
          "expected '{' to begin onDestroy method body");
  ContextGuard _(contexts, DeclContext::BLOCK);
  Stmt::Ptr stmt = blockStmt();
  auto end = previous();
  return make_shared<OnDestroyDecl>(makeSpan(t, end), stmt);
}

Decl::Ptr Parser::importDecl() {
  if (endImport) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P063);
    dia.labels = {
        {peek().span, "this import appears after a declaration", true},
    };
    dia.notes = {
        "all import declarations must be placed at the beginning of the file",
    };
    dia.helps = {
        "move this import before every non-import declaration",
    };
    engine.emit(dia);
  }

  SourceSpan span = peek().span;
  advance(); // import

  optional<StringDatum> module;
  vector<StringDatum> path;
  vector<ImportedType> types;

  while (!check(TKind::SEMICOLON) && !isAtEnd()) {
    if (check(TKind::LEFT_BRACE)) {
      if (path.empty()) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P067);
        dia.labels = {
            {peek().span,
             "a type can only be selected after the complete import path",
             true},
        };
        dia.notes = {
            "the type selector must appear at the end of an import path",
        };
        dia.helps = {
            "move the type selector after the final path segment",
        };
        engine.emit(dia);
        recover.recover();
      }

      advance(); // {

      if (check(TKind::RIGHT_BRACE)) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P041);
        dia.labels = {
            {peek().span, "expected a type name here", true},
        };
        engine.emit(dia);
        recover.recover();
      }

      while (!check(TKind::RIGHT_BRACE) && !isAtEnd()) {
        Token name = consume(TKind::IDENTIFIER, DiagnosticCode::HRD_P041,
                             "expected type name in import selector");

        if (check(TKind::CAST)) {
          advance(); // as 처리
          auto local = consume(TKind::IDENTIFIER, DiagnosticCode::HRD_P041,
                               "expected alias name in import selector");
          if (local.text == name.text) {
            auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P068);
            dia.labels = {
                {local.span,
                 "this alias is identical to the original type name", true},
                {name.span, "original type name is declared here", false},
            };
            dia.notes = {
                "an alias must introduce a different local name",
            };
            dia.helps = {
                "choose a different alias or remove the alias declaration",
            };
            engine.emit(dia);
            recover.recover();
          }
          types.push_back(
              {{name.text, name.span}, {local.text, local.span}, true});
        } else {
          types.push_back({{name.text, name.span}, {name.text, name.span}});
        }

        if (check(TKind::COMMA)) {
          advance();

          if (check(TKind::RIGHT_BRACE)) {
            auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P065);
            dia.labels = {
                {previous().span, "expected another type after this comma",
                 true},
            };
            dia.notes = {
                "a comma in a type list must separate two type names",
            };
            dia.helps = {
                "add a type after the comma or remove the trailing comma",
            };
            engine.emit(dia);
            recover.recover();
          }

          continue;
        }

        if (!check(TKind::RIGHT_BRACE)) {
          // 별도 expected ',' or '}' 진단 필요
          recover.recover();
        }
      }

      consume(TKind::RIGHT_BRACE, DiagnosticCode::HRD_P040,
              "expected '}' after imported types");
      break;
    }

    if (peek().isKeyword()) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P070);

      dia.labels = {
          {peek().span, "reserved keyword cannot be used as an import path",
           true},
      };

      dia.notes = {
          "import path segments must be valid identifiers",
      };

      dia.helps = {
          "rename the source file or path segment to a non-reserved identifier",
      };

      engine.emit(dia);
      recover.recover();
    }

    Token name = consume(TKind::IDENTIFIER, DiagnosticCode::HRD_P041,
                         "expected module name or path after 'import'");

    if (check(TKind::DOUBLE_COLON)) {
      if (module.has_value()) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P064);
        dia.labels = {
            {name.span, "additional module qualifier appears here", true},
            {module->span, "module qualifier was first specified here", false},
        };
        dia.notes = {
            "an import path can specify only one module name before '::'",
        };
        dia.helps = {
            "remove the additional module qualifier from the import path",
        };
        engine.emit(dia);
        recover.recover();
      }

      module = StringDatum(name.text, name.span);
      advance(); // ::
      continue;
    }

    path.emplace_back(name.text, name.span);

    if (check(TKind::DOT)) {
      if (following(1).isKeyword()) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P069);

        dia.labels = {
            {following(1).span,
             "reserved keyword cannot be used as an import path", true},
        };

        dia.notes = {
            "import path segments must be valid identifiers",
        };

        dia.helps = {
            "rename the source file or path segment to a non-reserved "
            "identifier",
        };

        engine.emit(dia);
        recover.recover();
      }

      if (!check(TKind::IDENTIFIER, 1) && !check(TKind::LEFT_BRACE, 1)) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P066);
        dia.labels = {
            {peek().span, "expected another path segment after this dot", true},
        };
        dia.notes = {
            "a dot in an import path must separate valid path elements",
        };
        dia.helps = {
            "add a path segment after '.' or remove the trailing dot",
        };
        engine.emit(dia);
        recover.recover();
      }

      advance(); // .
    }
  }

  consume(TKind::SEMICOLON, DiagnosticCode::HRD_P044,
          "expected ';' after import declaration");

  return make_shared<ImportDecl>(makeSpan(span, previous().span), module, path,
                                 types);
}