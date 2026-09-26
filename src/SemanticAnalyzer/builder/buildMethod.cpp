#include "hrd/AST/Decl.h"
#include "hrd/AST/Expr.h"
#include "hrd/Inputs.h"
#include "hrd/Recover/BuilderRecover.h"
#include "hrd/SemanticAnalyzer/Builder.h"
#include "hrd/SemanticAnalyzer/Scope.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/compiler/CompilerContexts.h"
#include "hrd/enums/MethodKind.h"
#include "hrd/util/Error.h"
#include "hrd/util/Guard.h"
#include <memory>
#include <optional>
#include <utility>

void Builder::visit(TraitSig *sig) {

  auto symbol = make_unique<MethodSymbol>();

  symbol->name = sig->name;
  symbol->decl = sig;
  symbol->owner = currentType;
  auto raw = symbol.get();

  auto result = table.add(std::move(symbol));

  if (!result.success) {
    switch (result.errorType) {
    case Result::DUPLICATED: {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S007);
      dia.labels = {
          {sig->span, "duplicate method declared here", true},
          {result.span, "previous method declared here", false},
      };
      dia.notes = {
          "method signatures must be unique within the same type",
      };
      engine.emit(dia);
    } break;

    case Result::RESERVED: {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S003);
      dia.labels = {
          {sig->span, "'" + sig->name + "'is a reserved identifier", true},
      };
      dia.notes = {
          {"reserved identifiers cannot be used in user declarations"}};
      engine.emit(dia);
      recover.recover();
      break;
    } break;

    case Result::UNKNOWN_SYMBOL:
      Error::internal(sig->span, "unknown symbol '" + sig->name + "'");
      break;

    case Result::NONE:
      break;
    }
  }
  ScopeGuard _(table);
  addGenericParam(sig->genericParams, raw);
  table.scopeManger.current()->scopeKind = Scope::ScopeKind::FUNC;
  for (auto &a : sig->params) {
    a->accept(this);
  }

  sig->symbol = raw;
}

void Builder::visit(FuncDecl *decl) {
  auto symbol = make_unique<MethodSymbol>();

  symbol->name = decl->name;
  symbol->decl = decl;
  symbol->owner = currentType;
  symbol->declType = currentType;
  symbol->modifier = decl->aModifier;
  symbol->module = table.moudle;
  symbol->path = table.registry.getCurrentFile()->path;
  symbol->isExtern = decl->prefix.isExtern;
  if (symbol->isExtern) {
    if (decl->prefix.linkName.has_value()) {
      symbol->linkName = decl->prefix.linkName.value();
    } else {
      symbol->linkName = decl->name;
    }
  }
  symbol->isFrame = decl->prefix.isFrame;
  symbol->isOverride = decl->prefix.isOverride;
  symbol->methodKind = MethodKind::Normal;
  symbol->isStatic = decl->prefix.isStatic;
  symbol->isGenericDecl = !decl->genericParams.empty();

  auto raw = symbol.get();
  auto result = table.add(std::move(symbol));

  if (!result.success) {
    switch (result.errorType) {
    case Result::DUPLICATED: {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S007);
      dia.labels = {
          {decl->span, "duplicate method declared here", true},
          {result.span, "previous method declared here", false},
      };
      dia.notes = {
          "method signatures must be unique within the same type",
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
  decl->methodSymbol = raw;

  ScopeGuard _(table);
  addGenericParam(decl->genericParams, raw);
  if (raw->isExtern && !decl->genericParams.empty()) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S142);
    dia.labels = {
        {decl->span, "this extern method declares generic parameters", true},
    };
    dia.notes = {
        "extern methods must bind to a concrete native function signature",
    };
    dia.helps = {
        "remove the generic parameters or provide concrete non-generic extern "
        "methods",
    };
    engine.emit(dia);
    recover.recover();
  }

  raw->scope = table.scopeManger.current();
  raw->scope->scopeKind = Scope::ScopeKind::FUNC;

  for (auto &a : decl->params) {
    a->accept(this);
  }

  if (!raw->isExtern) {
    decl->body->accept(this);
  }
}

void Builder::visit(Param *a) {
  auto s = make_unique<ParamSymbol>();
  s->name = a->name;
  s->kind = ValueSymbol::Kind::PARAM;
  s->node = a;
  s->nameSpan = a->span;
  auto r = s.get();
  auto re = table.add(std::move(s));

  if (!re.success) {
    switch (re.errorType) {
    case Result::DUPLICATED: {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S005);
      dia.labels = {
          {a->span, "duplicate variable declared here", true},
          {table.getType(a->name)->decl->span,
           "previous variable declared here", false},
      };
      dia.notes = {
          "variable names must be unique within the same scope",
      };
      engine.emit(dia);
    } break;

    case Result::RESERVED: {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S003);
      dia.labels = {
          {a->span, "'" + a->name + "'is a reserved identifier", true},
      };
      dia.notes = {
          {"reserved identifiers cannot be used in user declarations"}};
      engine.emit(dia);
      recover.recover();
      break;
    } break;

    case Result::UNKNOWN_SYMBOL:
      Error::internal(a->span, "unknown symbol '" + a->name + "'");
      break;

    case Result::NONE:
      break;
    }
  }
  a->symbol = r;
}

void Builder::visit(InitDecl *decl) {
  auto symbol = make_unique<MethodSymbol>();

  symbol->name = decl->name;
  symbol->decl = decl;
  symbol->owner = currentType;
  symbol->methodKind = MethodKind::Init;
  symbol->returnType = table.registry.getBuilt("void");
  symbol->module = table.moudle;
  symbol->path = table.registry.getCurrentFile()->path;
  auto raw = symbol.get();

  table.registry.addInit(std::move(symbol));

  decl->methodSymbol = raw;

  ScopeGuard _(table);

  raw->scope = table.scopeManger.current();
  table.scopeManger.current()->scopeKind = Scope::ScopeKind::INIT;
  raw->isOverride = decl->prefix.isOverride;

  for (auto &a : decl->params) {
    auto s = make_unique<ParamSymbol>();
    s->name = a->name;
    s->kind = ValueSymbol::Kind::PARAM;
    s->node = a.get();
    auto r = s.get();
    auto re = table.add(std::move(s));

    if (!re.success) {
      switch (re.errorType) {
      case Result::DUPLICATED: {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S005);
        dia.labels = {
            {a->span, "duplicate variable declared here", true},
            {table.getType(a->name)->decl->span,
             "previous variable declared here", false},
        };
        dia.notes = {
            "variable names must be unique within the same scope",
        };
        engine.emit(dia);
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
        Error::internal(a.get()->span,
                        "unknown symbol '" + a.get()->name + "'");
        break;

      case Result::NONE:
        break;
      }
    }
    a->symbol = r;
    a->type->accept(this);
  }

  decl->body->accept(this);
}

void Builder::visit(OnDestroyDecl *decl) {
  auto symbol = make_unique<MethodSymbol>();

  symbol->name = decl->name;
  symbol->decl = decl;
  symbol->owner = currentType;
  symbol->methodKind = MethodKind::OnDestroy;
  symbol->returnType = table.registry.getBuilt("void");
  symbol->module = table.moudle;
  symbol->path = table.registry.getCurrentFile()->path;
  auto raw = symbol.get();

  if (!table.registry.addOnDestroy(std::move(symbol))) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S007);
    dia.labels = {
        {decl->span, "duplicate onDestroy declared here", true},
        {table.getType(decl->name)->decl->span,
         "previous onDestroy declared here", false},
    };
    dia.notes = {
        "method signatures must be unique within the same type",
    };
    engine.emit(dia);
  }

  decl->methodSymbol = raw;

  ScopeGuard _(table);

  raw->scope = table.scopeManger.current();
  table.scopeManger.current()->scopeKind = Scope::ScopeKind::ONDESTROY;

  decl->body->accept(this);
}