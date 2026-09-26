#include "hrd/AST/Decl.h"
#include "hrd/AST/Expr.h"
#include "hrd/AST/Stmt.h"
#include "hrd/SemanticAnalyzer/Linker.h"
#include "hrd/SemanticAnalyzer/Scope.h"
#include "hrd/SemanticAnalyzer/SymbolTable/SymbolTable.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/compiler/CompilerContexts.h"
#include "hrd/util/Error.h"
#include "hrd/util/Guard.h"
#include "hrd/util/Helper.h"
#include <cassert>
#include <memory>
#include <utility>
#include <vector>

void Linker::visit(DeclStmt *stmt) { stmt->decl->accept(this); }

void Linker::visit(FuncDecl *decl) {

  if (decl->returnType == nullptr) {
    Error::internal(decl->span, "return ast node is nullptr");
  }

  decl->returnType->accept(this);
  decl->methodSymbol->returnType = decl->returnType->resolved;

  ScopeGuard _(table, decl->methodSymbol->scope);
  for (auto &p : decl->params) {
    p->accept(this);
    p->symbol->typeSymbol = p->type->resolved;
    decl->methodSymbol->params.push_back(p->symbol);
  }

  auto it = currentType->methodMap.find(decl->methodSymbol->name);
  if (it == currentType->methodMap.end()) {
    Error::internal(decl->span, "fail to find method map");
  }

  auto &bucket = it->second;
  auto raw = decl->methodSymbol;
  if (auto result = Helper::hasSameMethodSig(bucket, raw); result.result) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S018);
    dia.labels = {
        {raw->decl->span, "duplicate impl method declared here", true},
        {result.span, "previous impl method declared here", false}};
    engine.emit(dia);
    recover.recover();
  }

  unique_ptr<ValueSymbol> selfReceiver = make_unique<ValueSymbol>();
  selfReceiver->typeSymbol = currentType;
  selfReceiver->name = decl->name + "self";
  auto rawSelf = selfReceiver.get();

  table.registry.addSelf(std::move(selfReceiver));
  raw->selfReceiver = rawSelf;

  if (!raw->isExtern) {
    decl->body->accept(this);
  }
}

void Linker::visit(InitDecl *decl) {
  ScopeGuard _(table, decl->methodSymbol->scope);

  for (auto &p : decl->params) {
    p->accept(this);
    p->symbol->typeSymbol = p->type->resolved;
    decl->methodSymbol->params.push_back(p->symbol);
  }
  decl->body->accept(this);
}

void Linker::visit(OnDestroyDecl *decl) {
  ScopeGuard _(table, decl->methodSymbol->scope);
  decl->body->accept(this);
}

void Linker::visit(TraitSig *sig) {
  for (auto &p : sig->params) {
    p->accept(this);
    sig->symbol->params.push_back(p->symbol);
  }
  sig->type->accept(this);
  sig->symbol->returnType = sig->type->resolved;
}
void Linker::visit(Param *param) {
  param->type->accept(this);
  param->symbol->typeSymbol = param->type->resolved;
  if (param->defaultValue.has_value()) {
    auto expr = param->defaultValue.value().get();

    if (auto lit = dynamic_cast<LiteralExpr *>(expr)) {
      param->symbol->defaultValue = lit;
    } else if (auto call = dynamic_cast<CallExpr *>(expr)) {
      param->symbol->defaultValue = call;
    } else {
      Error::internal(param->span, "illegal defaultValue ast kind");
    }
  }
}
