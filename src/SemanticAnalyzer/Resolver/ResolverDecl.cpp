#include "AST/ASTNode.h"
#include "AST/Decl.h"
#include "AST/Expr.h"
#include "AST/Stmt.h"
#include "SemanticAnalyzer/ResolvedLit.h"
#include "SemanticAnalyzer/Resolver.h"
#include "SemanticAnalyzer/symbol/MethodSymbol.h"
#include "SemanticAnalyzer/symbol/Symbol.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "SemanticAnalyzer/symbol/ValueSymbol.h"
#include "util/Error.h"
#include "util/Guard.h"
#include "util/TypeResolver.h"
#include <llvm/ADT/APInt.h>
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
  for (auto a : decl->innerDecl) {
    a->accept(this);
  }
}

void Resolver::visit(StructDecl *decl) {
  if (!decl->symbol) {
    Error::internal(decl->token, "StructDecl symbol not initialized");
  }
  ScopeGuard _(*table, decl->symbol->memberScope);
  TypeContextGuard __(currentType, decl->symbol);
  for (auto a : decl->fields) {
    a->accept(this);
  }
}
void Resolver::visit(EnumDecl *) {}
void Resolver::visit(ImplDecl *decl) {
  auto implIt = table->implMap.find(decl);
  if (implIt == table->implMap.end()) {
    Error::internal(decl->token, "fail to find impl");
  }
  ScopeGuard _(*table, implIt->second->memberScope);
  TypeContextGuard __(currentType, implIt->second->target);
  auto prev = currentSelf;
  currentSelf = implIt->second->target->memberScope;
  for (auto &m : decl->LinkedImplMethods) {
    m->accept(this);
  }
  currentSelf = prev;
}

void Resolver::visit(TraitDecl *decl) {
  ScopeGuard _(*table, decl->symbol->memberScope);
  for (auto a : decl->traitSigs) {
    a->accept(this);
  }
}

void Resolver::visit(FuncDecl *decl) {

  ScopeGuard _(*table, decl->methodSymbol->scope);
  auto symbol = decl->methodSymbol;

  for (auto &p : decl->params) {
    p->accept(this);
    symbol->paramTypes.push_back(p->symbol->typeSymbol);
  }
  decl->body->accept(this);

  if (decl->returnType.has_value()) {
    auto rt = decl->returnType.value().get();
    rt->accept(this);
    auto type = rt->resolved;
    for (auto r : symbol->returns) {
      if (!isAssignable(type, r->returnType)) {
        Error::diagnostic(r->token, "unmatched return type");
      }
    }
    symbol->returnType = rt->resolved;
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
  if (decl->init) {
    decl->init->accept(this);
  }

  if (!decl->type->resolved) {
    Error::internal(decl->token, "decl->type->resolved is nullptr");
  }
  if (!decl->symbol->typeSymbol) {
    Error::internal(decl->token, "typeSymbol is nullptr");
  }
}

void Resolver::visit(TypeNode *type) {
  TypeResolver::resolveTypeNode(type, table);
}
void Resolver::visit(ASTNode *) {}

void Resolver::visit(TraitSig *sig) {

  for (auto p : sig->params) {
    p->accept(this);
    p->symbol->typeSymbol = p->type->resolved;
  }
}
void Resolver::visit(Param *param) {
  param->type->accept(this);
  param->symbol->typeSymbol = param->type->resolved;
  if (!param->symbol->typeSymbol) {
    Error::internal(param->token, "param type is unlinked");
  }
  if (param->defaultValue.has_value()) {
    param->defaultValue.value()->accept(this);
  }
}

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