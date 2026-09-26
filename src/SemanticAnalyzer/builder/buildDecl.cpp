#include "hrd/AST/Decl.h"
#include "hrd/AST/DeclContext.h"
#include "hrd/AST/Expr.h"
#include "hrd/Inputs.h"
#include "hrd/Recover/BuilderRecover.h"
#include "hrd/SemanticAnalyzer/Builder.h"
#include "hrd/SemanticAnalyzer/Scope.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/compiler/CompilerContexts.h"
#include "hrd/util/Error.h"
#include <memory>
#include <utility>

void Builder::visit(ImportDecl *decl) {
  vector<string> vec;
  for (auto s : decl->path) {
    vec.push_back(s.str);
  }
  decl->sPath = {std::move(vec)};
}

void Builder::visit(VarDecl *decl) {
  auto symbol = make_unique<ValueSymbol>();
  symbol->name = decl->name;
  symbol->kind = ValueSymbol::Kind::VAR;
  symbol->node = decl;
  symbol->isRoot = decl->isRoot;
  symbol->modifier = decl->aModifier;
  symbol->nameSpan = decl->span;
  symbol->isConst = !decl->isMutable;
  auto raw = symbol.get();

  auto result = table.add(std::move(symbol));

  if (decl->isRoot) {
    if (!result.success) {
      switch (result.errorType) {
      case Result::DUPLICATED: {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S005);
        dia.labels = {
            {decl->span, "duplicate variable declared here", true},
            {result.span, "previous variable declared here", false},
        };
        dia.notes = {
            "variable names must be unique within the root",
        };
        engine.emit(dia);
        recover.recover();
      } break;

      case Result::RESERVED: {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S003);
        dia.labels = {
            {decl->span, "'" + decl->name + "'is a reserved identifier", true},
        };
        dia.notes = {
            {"reserved identifiers cannot be used in user declarations"}};
        engine.emit(dia);
        recover.recover();
        break;
      } break;

      case Result::UNKNOWN_SYMBOL:
        Error::internal(decl->span, "unknown symbol '" + decl->name + "'");
        break;

      case Result::NONE:
        break;
      }
    }
  } else {
    if (!result.success) {
      switch (result.errorType) {
      case Result::DUPLICATED: {
        if (decl->context == DeclContext::CLASSBODY) {
          auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S006);
          dia.labels = {
              {decl->span, "duplicate field declared here", true},
              {result.span, "previous field declared here", false},
          };
          dia.notes = {
              "field names must be unique within the same type",
          };
          engine.emit(dia);
        } else {
          auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S005);
          dia.labels = {
              {decl->span, "duplicate variable declared here", true},
              {result.span, "previous variable declared here", false},
          };
          dia.notes = {
              "variable names must be unique within the same scope",
          };
          engine.emit(dia);
        }

        recover.recover();
      } break;

      case Result::RESERVED: {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S003);
        dia.labels = {
            {decl->span, "'" + decl->name + "'is a reserved identifier", true},
        };
        dia.notes = {
            {"reserved identifiers cannot be used in user declarations"}};
        engine.emit(dia);
        recover.recover();
        break;
      } break;

      case Result::UNKNOWN_SYMBOL:
        Error::internal(decl->span, "unknown symbol '" + decl->name + "'");
        break;

      case Result::NONE:
        break;
      }
    }
  }

  decl->symbol = raw;

  if (decl->init) {
    decl->init->accept(this);
  }
}
