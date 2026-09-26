#include "hrd/AST/Decl.h"
#include "hrd/Recover/BuilderRecover.h"
#include "hrd/SemanticAnalyzer/Builder.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/compiler/CompilerContexts.h"
#include "hrd/util/Error.h"
#include <cstddef>
#include <memory>
#include <utility>

void Builder::addGenericParam(TypeName &type, GenericOnwer *owner) {
  addGenericParam(type.genericParams, owner);
}

void Builder::addGenericParam(vector<GenericParamDecl> &params,
                              GenericOnwer *owner) {
  size_t index = 0;
  for (auto &p : params) {
    auto symbol = make_unique<GenericParamSymbol>(p.name, owner, index++);
    auto raw = symbol.get();
    if (!owner->addGenericParam(std::move(symbol))) {
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
    table.scopeManger.addGenericParam(raw);
  }
}

void Builder::checkGenericShadowing(vector<GenericParamDecl> &params) {
  auto &map = currentType->getGenericParamMap();
  for (auto &a : params) {
    auto name = a.name.text;
    auto it = map.find(name);
    if (it != map.end()) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S141);
      dia.labels = {
          {a.name.span, "this declaration shadows a generic parameter", true},
          {it->second->span, "the generic parameter was declared here", false},
      };
      dia.notes = {
          "generic parameter names remain visible throughout their generic "
          "scope",
      };
      dia.helps = {
          "rename this declaration to avoid shadowing the generic parameter",
      };
      engine.emit(dia);
      recover.recover();
    }
  }
}