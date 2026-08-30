#include "hrd/Parser.h"
#include "hrd/AST/Decl.h"
#include "hrd/AST/DeclContext.h"
#include "hrd/AST/TokenStream.h"
#include "hrd/Recover/ParserRecover.h"
#include "hrd/Token.h"
#include "hrd/compiler/CompilerContexts.h"
#include "hrd/util/Error.h"
#include <memory>

using ptr = shared_ptr<ASTNode>;

Parser::Parser(ParserContext &ctx)
    : tokens(ctx.tokenStream.tokens), engine(ctx.engine),
      stream(ctx.tokenStream), recover(*this) {}

vector<Decl::Ptr> Parser::parse() {

  vector<Decl::Ptr> decls;

  while (!isAtEnd()) {
    Token t = peek();
    auto decl = declaration(DeclContext::TOPLEVEL);
    decls.push_back(decl);
  }
  return decls;
}

Decl::Ptr Parser::declaration(DeclContext context) {
  DeclPrefix prefix = {};
  prefix.startToken = peek();

  ContextGuard _{contexts, context};

  bool checkAceess = false;

  if (check(TKind::IMPORT)) {
    return importDecl();
  }

  while (isAccessModifier()) {
    if (checkAceess) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P008);
      dia.labels = {
          {peek().span, "an access modifier was already specified", true},
      };
      engine.emit(dia);
      recover.recover();
    }
    endImport = true;
    if (context != DeclContext::CLASSBODY) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P001);
      dia.labels = {
          {peek().span, "only fields may have an access modifier", true},
      };
      engine.emit(dia);
      recover.recover();
    }
    prefix.modi = AModifierConvertor(advance());
    checkAceess = true;
  }

  while (check({
      TKind::CONST,
      TKind::ROOT,
      TKind::FRAME,
      TKind::OVERRIDE,
      TKind::ASYNC,
  })) {
    if (check(TKind::CONST)) {
      advance();
      if (prefix.isConst) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P002);
        dia.labels = {
            {peek().span, "'const' modifier is already specified", true},
        };
        engine.emit(dia);
        recover.recover();
      }

      prefix.isConst = true;
    }
    if (check(TKind::ROOT)) {
      advance();
      if (prefix.isRoot) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P003);
        dia.labels = {
            {peek().span, "'root' modifier is already specified", true},
        };
        engine.emit(dia);
        recover.recover();
      }

      prefix.isRoot = true;
    }
    if (check(TKind::FRAME)) {
      advance();
      if (prefix.isFrame) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P004);
        dia.labels = {
            {peek().span, "'frame' modifier is already specified", true},
        };
        engine.emit(dia);
        recover.recover();
      }
      prefix.isFrame = true;
    }
    if (check(TKind::OVERRIDE)) {
      advance();
      if (prefix.isOverride) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P005);
        dia.labels = {
            {peek().span, "'override' modifier is already specified", true},
        };
        engine.emit(dia);
        recover.recover();
      }
      prefix.isOverride = true;
    }
    if (check(TKind::ASYNC)) {
      advance();
      if (prefix.isAsync) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P006);
        dia.labels = {
            {peek().span, "'async' modifier is already specified", true},
        };
        engine.emit(dia);
        recover.recover();
      }
      prefix.isAsync = true;
    }
  }
  // if (check(TKind::FUNC)) {
  //   return functionDecl(prefix, true);
  // }

  if (check(TKind::VOID)) {
    return functionDecl(prefix);
  }

  if (check(TKind::INIT)) {

    return initDecl(prefix);
  }

  if (check(TKind::ONDESTROY)) {
    return onDestroyDecl(prefix);
  }

  if (isType()) {
    if (isFunc()) {
      return functionDecl(prefix);
    } else {
      return varDecl(prefix);
    }
  }

  if (check(TKind::HANDLE)) {
    return handleDecl(prefix);
  }

  if (check(TKind::CLASS)) {
    return classDecl(prefix);
  }

  if (check(TKind::STRUCT)) {
    return structDecl(prefix);
  }

  if (check(TKind::IMPL)) {
    return implDecl(prefix);
  }

  if (check(TKind::TRAIT)) {
    return traitDecl(prefix);
  }

  if (check(TKind::ENUM)) {
    return enumDecl(prefix);
  }

  auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P007);
  dia.labels = {
      {peek().span, "expected a declaration here", true},
  };
  engine.emit(dia);
  recover.recover();
  return nullptr;
}

Stmt::Ptr Parser::statement() {
  switch (peek().kind) {
  case TKind::SEMICOLON: {
    return make_shared<EmptyStmt>(advance().span);
  }
  case TKind::IF:
    return ifStmt();
  case TKind::SWITCH:
    return switchStmt();
  case TKind::FOR:
    return forStmt();
  case TKind::WHILE:
    return whileStmt();
  case TKind::BREAK: {
    auto tk = advance();
    return make_shared<BreakStmt>(tk.span, tk);
  }

  case TKind::CONTINUE: {
    auto tk = advance();
    return make_shared<ContinueStmt>(tk.span, tk);
  }

  case TKind::RETURN:
    return returnStmt();

  case TKind::ROOT:
    if (following().kind == TKind::DOT) {
      return expressionStmt();
    } else {
      return declStmt();
    }
  case TKind::INT:
  case TKind::FLOAT:
  case TKind::STRING:
  case TKind::CHAR:
  case TKind::FIXED:
  case TKind::BOOL:
  case TKind::PUBLIC:
  case TKind::PROTECTED:
  case TKind::PRIVATE:
  case TKind::IMPL:
  case TKind::TRAIT:
  case TKind::CONST:
  case TKind::HANDLE:
  case TKind::FRAME:
  case TKind::INIT:
  case TKind::IMPORT:
    return declStmt();
  case TKind::DOUBLE_ANGLEBUCKET:
    return valueTransferStmt();
  case TKind::TRY:
    return tryStmt();

    // case TKind::ONEXIT:
    //   return onexitStmt();

  case TKind::THROW:
    return throwStmt();
  case TKind::LEFT_BRACE:
    advance();
    return blockStmt();

  case TKind::IDENTIFIER:
    if (looksLikeDecl()) {
      return declStmt();
    } else {
      return expressionStmt();
    }

  default:
    return expressionStmt();
  }
}

Expr::Ptr Parser::expression() {
  Expr::Ptr expr = assignment();
  return expr;
}