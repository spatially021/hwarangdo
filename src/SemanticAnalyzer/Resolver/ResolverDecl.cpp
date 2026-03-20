#include "AST/ASTNode.h"
#include "AST/Decl.h"
#include "AST/Stmt.h"
#include "SemanticAnalyzer/Guard.h"
#include "SemanticAnalyzer/Resolver.h"
#include "SemanticAnalyzer/symbol/MethodSymbol.h"
#include "SemanticAnalyzer/symbol/Symbol.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "SemanticAnalyzer/symbol/ValueSymbol.h"
#include "util/Error.h"
#include <string>

void Resolver::visit(ClassDecl *decl) {
  if (!decl->symbol) {
    Error::internal(decl->token, "StructDecl symbol not initialized");
  }
  ScopeGuard _(*table, decl->symbol->memberScope);
  TypeContextGuard __(currentType, decl->symbol);

  for (auto a : decl->fields)
    a->accept(this);
  for (auto a : decl->methods) {
    a->accept(this);
  }
  for (auto a : decl->innterDecl) {
    a->accept(this);
  }
}

void Resolver::visit(StructDecl *decl) {
  if (!decl->symbol) {
    Error::internal(decl->token, "StructDecl symbol not initialized");
  }
  ScopeGuard _(*table, decl->symbol->memberScope);
  for (auto a : decl->fields) {
    a->accept(this);
  }
}
void Resolver::visit(EnumDecl *) {}
void Resolver::visit(ImplDecl *) {}

void Resolver::visit(TraitDecl *decl) {
  ScopeGuard _(*table, decl->symbol->memberScope);
  for (auto a : decl->traitSigs) {
    a->accept(this);
  }
}

void Resolver::visit(FuncDecl *decl) {

  ScopeGuard _(*table, decl->methodSymbol->scope);
  decl->body->accept(this);

  auto symbol = decl->methodSymbol;

  if (decl->returnType.has_value()) {
    auto rt = decl->returnType.value().get();
    rt->accept(this);
    auto type = rt->resolved;
    for (auto r : symbol->returns) {
      if (!isAssignable(type, r->returnType)) {
        Error::diagnostic(r->token, "unmatched return type");
      }
    }
  } else {
    if (symbol->returns.empty()) {
      symbol->returnType = table->getType("void");
    } else {
      auto rt = symbol->returns[0]->returnType;
      for (auto r : symbol->returns) {
        if (!isAssignable(rt, r->returnType)) {
          Error::diagnostic(r->token, "unmatched return type");
        }
      }
      symbol->returnType = rt;
    }
  }
}
void Resolver::visit(VarDecl *decl) {
  if (!decl->symbol) {
    Error::internal(decl->token, "var symbol is nullptr");
  }
  if (decl->init) {
    decl->init->accept(this);
  }
}
void Resolver::visit(ArrayDecl *) {}

void Resolver::visit(TypeNode *) {}
void Resolver::visit(ASTNode *) {}

void Resolver::visit(TraitSig *sig) {
  sig->type->accept(this);
  for (auto p : sig->params) {
    p->accept(this);
    p->symbol->typeSymbol = p->type->resolved;
  }
}
void Resolver::visit(Param *) {}

void Resolver::visit(InitDecl *decl) {
  auto symbol = decl->methodSymbol;
  for (auto r : symbol->returns) {
    if (r->returnType != table->getBuilt("void")) {
      Error::diagnostic(r->token, "in init cannot declare a return type");
    }
  }

  currentMethod = decl->methodSymbol;
  ScopeGuard _(*table, decl->methodSymbol->scope);
  decl->body->accept(this);
}