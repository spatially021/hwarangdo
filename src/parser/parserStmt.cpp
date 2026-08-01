#include "hrd/AST/Decl.h"
#include "hrd/AST/Expr.h"
#include "hrd/AST/Stmt.h"
#include "hrd/Parser.h"
#include "hrd/SourceSpan.h"
#include "hrd/Token.h"
#include "hrd/util/Error.h"
#include "hrd/util/diagnostic/Diagnostic.h"
#include <memory>
#include <optional>

using Ptr = Stmt::Ptr;
using namespace std;

Ptr Parser::expressionStmt() {
  Token t = peek();
  Expr::Ptr expr = expression();
  consume(TKind::SEMICOLON, DiagnosticCode::HRD_P044,
          "expected ';' after expression");
  return make_shared<ExprStmt>(makeSpan(t.span, expr->span), expr);
}

Ptr Parser::declStmt() {
  Token t = peek();
  Decl::Ptr decl = declaration(contexts.back());
  return make_shared<DeclStmt>(makeSpan(t.span, decl->span), decl);
}

Ptr Parser::ifStmt() {
  Token t = peek();
  advance(); // if처리
  consume(TKind::LEFT_PAREN, DiagnosticCode::HRD_P046,
          "expected '(' after 'if'");

  Expr::Ptr condition = expression();

  consume(TKind::RIGHT_PAREN, DiagnosticCode::HRD_P047,
          "expected ')' to close condition");

  Ptr thenBranch = bodyStmt();
  Ptr elseBranch = nullptr;
  if (check(TKind::ELSE)) {
    advance(); // else 처리
    elseBranch = bodyStmt();
  }

  return make_shared<IfStmt>(
      makeSpan(t.span, elseBranch ? elseBranch->span : thenBranch->span),
      condition, thenBranch, elseBranch);
}

Ptr Parser::blockStmt() {
  Token t = peek();
  vector<Ptr> s;
  while (!check(TKind::RIGHT_BRACE) && !isAtEnd()) {
    s.push_back(statement());
  }

  consume(TKind::RIGHT_BRACE, DiagnosticCode::HRD_P040,
          "expected '}' at end of block");
  auto end = peek();
  return make_shared<BlockStmt>(makeSpan(t, end), s);
}

Ptr Parser::bodyStmt() {
  ContextGuard _{contexts, DeclContext::BLOCK};
  if (check(TKind::LEFT_BRACE)) {
    advance(); //{처리
    return blockStmt();
  }
  return statement();
}

Ptr Parser::forStmt() {
  Token t = peek();
  advance(); // for 처리
  consume(TKind::LEFT_PAREN, DiagnosticCode::HRD_P046,
          "expected '(' after 'for'");

  Ptr init = declStmt();
  auto decl = dynamic_pointer_cast<DeclStmt>(init);
  if (decl == nullptr) {
    Error::internal(t, "for's var init is not declStmt");
  }
  auto var = dynamic_cast<VarDecl *>(decl->decl.get());
  if (var == nullptr) {
    Error::internal(t, "for's var inti is not varDecl");
  }

  if (var->init != nullptr) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P036);
    dia.labels = {
        {peek().span, "initializer is not allowed here", true},
    };
    engine.emit(dia);
    recover.recover();
  }

  Expr::Ptr from = expression();
  consume(TKind::DOUBLE_DOT, DiagnosticCode::HRD_P055,
          "expected '..' between range bounds");
  Expr::Ptr to = expression();
  Expr::Ptr step = nullptr;
  if (check(TKind::BY)) {
    advance(); // advance by
    step = expression();
  }
  consume(TKind::RIGHT_PAREN, DiagnosticCode::HRD_P047,
          "expected ')' to close condition");
  shared_ptr<Range> range =
      make_shared<Range>(makeSpan(from->span, to->span), from, to, step);
  Ptr body = bodyStmt();
  return make_shared<ForStmt>(makeSpan(t.span, body->span), decl, range, body);
}

Ptr Parser::whileStmt() {
  Token t = peek();
  advance(); // while처리
  consume(TKind::LEFT_PAREN, DiagnosticCode::HRD_P046,
          "expected '(' after 'while'");

  Expr::Ptr conditon = expression();

  consume(TKind::RIGHT_PAREN, DiagnosticCode::HRD_P047,
          "expected ')' to close condition");
  Ptr body = bodyStmt();
  return make_shared<WhileStmt>(makeSpan(t.span, body->span), conditon, body);
}

Ptr Parser::switchStmt() {
  Token t = peek();
  advance(); // switch처리
  consume(TKind::LEFT_PAREN, DiagnosticCode::HRD_P046,
          "expected '(' after 'switch'");

  Expr::Ptr value = expression();

  consume(TKind::RIGHT_PAREN, DiagnosticCode::HRD_P047,
          "expected ')' to close condition");
  consume(TKind::LEFT_BRACE, DiagnosticCode::HRD_P043,
          "expected '{' begin switch body");

  vector<shared_ptr<Case>> cases;

  while (!check(TKind::RIGHT_BRACE) && !isAtEnd()) {
    auto c = caseStmt();
    cases.push_back(c);
  }

  consume(TKind::RIGHT_BRACE, DiagnosticCode::HRD_P040,
          "expected '}' after switch body");
  auto end = peek();
  return make_shared<SwitchStmt>(makeSpan(t, end), value, cases);
}

shared_ptr<Case> Parser::caseStmt(bool isSwtich) {
  Token tok = peek();
  if (check(TKind::CASE)) {
    advance(); // case 처리
    vector<Expr::Ptr> values;
    while (!check(TKind::EQAUL_AGNLEBUCKET) && !isAtEnd()) {

      auto temp = dynamic_pointer_cast<CaseValueExpr>(parseCaseValue());
      if (!temp) {
        Error::internal("illegal expr kind");
      }
      values.push_back(temp);
      if (check(TKind::COMMA)) {
        if (check(TKind::EQAUL_AGNLEBUCKET, 1)) {
          auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P037);
          dia.labels = {
              {peek().span, "expected case selector after ','", true},
          };
          engine.emit(dia);
          recover.recover();
        } else
          advance(); //,처리
      }
    }
    consume(TKind::EQAUL_AGNLEBUCKET, DiagnosticCode::HRD_P056,
            "expected '=>' after case selector");
    Ptr body;
    if (check(TKind::LEFT_BRACE)) {
      body = bodyStmt();
    } else {
      consume(TKind::LEFT_BRACE, DiagnosticCode::HRD_P043,
              "expect '{' begin case body");
    }

    auto end = peek();
    return make_shared<Case>(makeSpan(tok, end), values, body);
  } else if (check(TKind::DEFAULT)) {
    if (!isSwtich) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P062);
      dia.labels = {
          {tok.span, "'default' is not allowed in match expressions", true},
      };
      dia.notes = {
          "match expressions use wildcard selectors instead of default clauses",
      };
      dia.helps = {
          "replace 'default' with '_' in match expressions",
      };
      engine.emit(dia);
      recover.recover();
    }
    advance(); // default 처리
    consume(TKind::EQAUL_AGNLEBUCKET, DiagnosticCode::HRD_P056,
            "expected '=>' after default");
    vector<Expr::Ptr> values;
    Ptr body;
    if (check(TKind::LEFT_BRACE))
      body = bodyStmt();
    else {
      consume(TKind::LEFT_BRACE, DiagnosticCode::HRD_P043,
              "expect '{' begin case body");
    }

    return make_shared<Case>(makeSpan(tok.span, body->span), values, body,
                             true);
  } else {
    if (isSwtich) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P038);
      dia.labels = {
          {peek().span,
           "only 'case' and 'default' declarations are allowed here", true},
      };
      engine.emit(dia);
      recover.recover();
    } else {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P038);
      dia.labels = {
          {peek().span,
           "only 'case' declarations and wildcard selectors are allowed here",
           true},
      };
      engine.emit(dia);
      recover.recover();
    }
  }
  Error::internal("unreachable");
}

Ptr Parser::returnStmt() {
  Token t = peek();
  advance(); // return 처리;
  Expr::Ptr expr;
  if (check(TKind::SEMICOLON))
    expr = nullptr;
  else
    expr = expression();
  consume(TKind::SEMICOLON, DiagnosticCode::HRD_P044,
          "expected ';' after return statement");
  auto end = peek();
  return make_shared<ReturnStmt>(makeSpan(t, end), expr);
}

Ptr Parser::valueTransferStmt() {
  Token t = peek();
  advance(); //<<처리
  Expr::Ptr expr = expression();
  consume(TKind::SEMICOLON, DiagnosticCode::HRD_P044,
          "expected ';' after value trasfer statement");
  auto end = peek();
  return make_shared<ValueTransferStmt>(makeSpan(t, end), expr);
}

Ptr Parser::tryStmt() {
  Token t = peek();
  advance(); // try 처리
  Ptr body = bodyStmt();
  vector<shared_ptr<CatchClause>> catches;
  while (check(TKind::CATCH))
    catches.push_back(dynamic_pointer_cast<CatchClause>(catchStmt()));
  auto end = peek();
  return make_shared<TryCatchStmt>(makeSpan(t, end), body, catches);
}

Ptr Parser::catchStmt() {
  Token t = peek();
  advance(); // catch 처리
  consume(TKind::LEFT_PAREN, DiagnosticCode::HRD_P046,
          "expected '(' after 'catch'");

  auto type = parseType();
  optional<string> name = nullopt;
  if (!check(TKind::RIGHT_BRACE))
    name = consume(TKind::IDENTIFIER, DiagnosticCode::HRD_P045,
                   "expected error variable name after type")
               .text;

  consume(TKind::RIGHT_PAREN, DiagnosticCode::HRD_P047,
          "expected ')' to close arugment list");
  Ptr body = bodyStmt();

  auto end = peek();
  return make_shared<CatchClause>(makeSpan(t, end), type, name, body);
}

// Ptr Parser::onexitStmt() {
//   Token t = peek();
//   advance(); // onexit 처리
//   consume(TKind::LEFT_BRACE, "expected '{' after 'onexit'");
//   Ptr body = blockStmt();
//   return make_shared<OnexitStmt>(makeSpan(t.span, body->span), body);
// }

Ptr Parser::throwStmt() {
  Token t = peek();
  advance(); // throw 처리
  Expr::Ptr expr = expression();
  return make_shared<ThrowStmt>(makeSpan(t.span, expr->span), expr);
}