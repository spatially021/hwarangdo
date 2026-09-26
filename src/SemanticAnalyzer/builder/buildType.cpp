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
#include "hrd/util/Error.h"
#include "hrd/util/Guard.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

void Builder::buildMain(ClassDecl *decl) {

  auto symbol = make_unique<MainSymbol>();
  symbol->name = decl->name;
  symbol->decl = decl;
  symbol->path = table.registry.getCurrentFile()->path;
  symbol->module = table.moudle;
  symbol->isGenericDecl = !decl->typeName.genericParams.empty();

  auto raw = symbol.get();

  auto result = table.add(std::move(symbol));

  if (!result.success) {
    switch (result.errorType) {
    case Result::DUPLICATED: {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S001);
      dia.labels = {
          {decl->span, "duplicate 'Main' class declared here", true},
          {table.main->decl->span, "previous 'Main' class declared here",
           false},
      };
      dia.notes = {{"only one 'Main' class may be declared in a program"}};
      engine.emit(dia);
      recover.recover();
    } break;

    case Result::RESERVED:
      Error::internal(decl->span, "unreachable");
      break;

    case Result::UNKNOWN_SYMBOL:
      Error::internal(decl->span, "unknown symbol '" + decl->name + "'");
      break;

    case Result::NONE:
      break;
    }
  }
  decl->symbol = raw;
  table.main = raw;
  size_t index = 0;
  for (auto &p : decl->typeName.genericParams) {
    if (!raw->addGenericParam(
            make_unique<GenericParamSymbol>(p.name, raw, index++))) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S137);
      dia.labels = {
          {p.name.span, "this generic parameter name is already declared",
           true},
      };
      dia.notes = {
          "generic parameter names must be unique within the same declaration",
      };
      dia.helps = {
          "rename one of the duplicate generic parameters",
      };
      engine.emit(dia);
      recover.recover();
    }
  }

  TypeContextGuard _(currentType, raw);
  ScopeGuard __(table);
  addGenericParam(decl->typeName, raw);

  raw->memberScope = table.scopeManger.current();
  table.scopeManger.current()->scopeKind = Scope::ScopeKind::FIELD;
  for (auto &a : decl->fields) {
    a->accept(this);
    raw->fields.push_back(a->symbol);
  }
  for (auto &a : decl->methods) {
    a->accept(this);
  }
}

void Builder::visit(ClassDecl *decl) {
  if (decl->name == "Main") {
    buildMain(decl);
    return;
  }

  auto symbol = make_unique<ObjectType>(TypeKind::CLASS);
  symbol->name = decl->name;
  symbol->decl = decl;
  if (decl->baseClass.has_value()) {
    decl->baseClass.value()->accept(this);
    symbol->baseName = decl->baseClass.value()->type;
  } else {
    symbol->baseName = nullopt;
  }
  symbol->isGenericDecl = !decl->typeName.genericParams.empty();

  symbol->path = table.registry.getCurrentFile()->path;
  symbol->module = table.moudle;
  auto raw = symbol.get();

  auto result = table.add(std::move(symbol));

  if (!result.success) {
    switch (result.errorType) {
    case Result::DUPLICATED: {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S002);
      dia.labels = {
          {decl->span, "type '" + decl->name + "' is already declared", true},
          {result.span, "previous declaration of '" + decl->name + "' is here",
           false},
      };
      dia.notes = {
          "type names must be unique within the same scope",
      };
      engine.emit(dia);
      recover.recover();
      break;
    }

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
    }
    case Result::UNKNOWN_SYMBOL:
      Error::internal(decl->span, "unknown symbol '" + decl->name + "'");
      break;

    case Result::NONE:
      break;
    }
  }

  decl->symbol = raw;

  TypeContextGuard _(currentType, raw);
  ScopeGuard __(table);
  addGenericParam(decl->typeName, raw);
  table.scopeManger.current()->scopeKind = Scope::ScopeKind::FIELD;
  raw->memberScope = table.scopeManger.current();
  for (auto &a : decl->fields) {
    a->accept(this);
    raw->fields.push_back(a->symbol);
  }
  for (auto &a : decl->methods) {
    a->accept(this);
  }
}

void Builder::visit(StructDecl *decl) {
  auto symbol = make_unique<ObjectType>(TypeKind::STRUCT);
  symbol->name = decl->name;
  symbol->decl = decl;
  symbol->path = table.registry.getCurrentFile()->path;
  symbol->module = table.moudle;
  auto raw = symbol.get();
  symbol->isGenericDecl = !decl->typeName.genericParams.empty();

  auto result = table.add(std::move(symbol));

  if (!result.success) {
    switch (result.errorType) {
    case Result::DUPLICATED: {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S002);
      dia.labels = {
          {decl->span, "type '" + decl->name + "' is already declared", true},
          {result.span, "previous declaration of '" + decl->name + "' is here",
           false},
      };
      dia.notes = {
          "type names must be unique within the same scope",
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

  decl->symbol = raw;

  TypeContextGuard _(currentType, raw);
  ScopeGuard __(table);
  addGenericParam(decl->typeName, raw);
  table.scopeManger.current()->scopeKind = Scope::ScopeKind::FIELD;
  raw->memberScope = table.scopeManger.current();
  for (auto &a : decl->fields) {
    a->accept(this);
    raw->fields.push_back(a->symbol);
  }

  for (auto &i : decl->inits) {
    i->accept(this);
  }
}

void Builder::visit(EnumDecl *decl) {

  auto symbol = make_unique<EnumType>();
  symbol->name = decl->name;
  symbol->decl = decl;
  symbol->path = table.registry.getCurrentFile()->path;
  symbol->module = table.moudle;
  symbol->isGenericDecl = !decl->typeName.genericParams.empty();

  auto raw = symbol.get();

  auto result = table.add(std::move(symbol));

  if (!result.success) {
    switch (result.errorType) {
    case Result::DUPLICATED: {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S002);
      dia.labels = {
          {decl->span, "type '" + decl->name + "' is already declared", true},
          {result.span, "previous declaration of '" + decl->name + "' is here",
           false},
      };
      dia.notes = {
          "type names must be unique within the same scope",
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

  decl->symbol = raw;

  TypeContextGuard _(currentType, raw);
  ScopeGuard __(table);
  addGenericParam(decl->typeName, raw);
  table.scopeManger.current()->scopeKind = Scope::ScopeKind::FIELD;
  raw->scope = table.scopeManger.current();

  uint32_t ordinal = 0;
  for (auto a : decl->variants) {
    auto v = make_unique<EnumVariantSymbol>();
    v->name = a->name;
    v->ordinal = ordinal++;
    if (raw->variantMap.count(v->name)) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S004);
      dia.labels = {
          {decl->span, "duplicate enum variant declared here", true},
          {result.span, "previous enum variant declared here", false},
      };
      dia.notes = {
          "an enum cannot contain multiple variants with the same name",
      };
      engine.emit(dia);
      recover.recover();
    }
    EnumVariantSymbol *r = v.get();
    v->typeSymbol = raw;
    raw->variants.push_back(std::move(v));
    raw->variantMap.emplace(r->name, r);
    a->symbol = r;
  }
}

void Builder::visit(ImplDecl *decl) {
  auto symbol = make_unique<ImplSymbol>();
  symbol->targetName = decl->target.name.text;
  symbol->decl = decl;

  auto raw = symbol.get();
  table.registry.addImpl(decl, std::move(symbol));
  decl->symbol = raw;

  TypeContextGuard __(currentType, raw);
  ScopeGuard _(table);
  raw->scope = table.scopeManger.current();
  table.scopeManger.current()->scopeKind = Scope::ScopeKind::FIELD;

  for (auto &a : decl->LinkedImplMethods) {
    a->accept(this);
    a->methodSymbol->owner = nullptr;
  }
}

void Builder::visit(TraitDecl *decl) {
  auto symbol = make_unique<TraitType>();
  symbol->name = decl->name;
  symbol->decl = decl;
  symbol->module = table.moudle;
  auto raw = symbol.get();

  auto result = table.add(std::move(symbol));

  if (!result.success) {
    switch (result.errorType) {
    case Result::DUPLICATED: {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S002);
      dia.labels = {
          {decl->span, "type '" + decl->name + "' is already declared", true},
          {result.span, "previous declaration of '" + decl->name + "' is here",
           false},
      };
      dia.notes = {
          "type names must be unique within the same scope",
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

  decl->symbol = raw;

  TypeContextGuard _(currentType, raw);
  table.scopeManger.current()->scopeKind = Scope::ScopeKind::FIELD;
  for (auto &a : decl->traitSigs) {
    if (a == nullptr)
      Error::internal("traitSig is nullptr");
    a->accept(this);
  }
}
