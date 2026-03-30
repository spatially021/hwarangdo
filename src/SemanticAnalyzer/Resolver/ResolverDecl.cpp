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
  for (auto &m : decl->LinkedImplMethods) {
    m->accept(this);
  }
}

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
  decl->type->accept(this);

  decl->symbol->typeSymbol = decl->type->resolved;

  if (!decl->type->resolved) {
    Error::internal(decl->token, "decl->type->resolved is nullptr");
  }
  if (!decl->symbol->typeSymbol) {
    Error::internal(decl->token, "typeSymbol is nullptr");
  }
}

void Resolver::visit(TypeNode *type) {
  if (dynamic_cast<BuiltinTypeNode *>(type) ||
      dynamic_cast<IdentifierTypeNode *>(type)) {
    auto symbol = table->getType(type);
    if (!symbol)
      Error::diagnostic(type->token, "unknown type : " + type->token.text);
    type->resolved = symbol;
  } else if (auto a = dynamic_cast<ArrayTypeNode *>(type)) {

    a->elementType->accept(this);
    llvm::APInt size = resolveFixedArraySize(a->fixedSize.get());
    a->resolved = table->arrayTypeGetOrCreate(a->elementType->resolved, size);

  } else if (auto g = dynamic_cast<GenericTypeNode *>(type)) {
    auto ar = g->typeArgs;
    TypeSymbol *orign = nullptr;
    vector<TypeSymbol *> args;
    for (auto &t : g->typeArgs) {
      t->accept(this);
      if (!t->resolved) {
        Error::internal(t->token,
                        "fail to resolve args Type : " + t->token.text);
      }
      args.push_back(t->resolved);
    }

    switch (g->gKind) {
    case GenericTypeNode::GenericKind::HANDLE:

      if (args.size() != 1) {
        Error::diagnostic(g->token, "Handle need one type but '" +
                                        to_string(args.size()) + "'");
      }

      if (ar[0]->resolved->kind == TypeSymbol::TypeKind::PRIMITIVE) {
        Error::diagnostic(ar[0]->token, "not allowed handle target type : " +
                                            ar[0]->token.text);
      }
      if (ar[0]->resolved->type == Symbol::SymbolType::MAIN) {
        Error::diagnostic(ar[0]->token, "not allowed handle target type : " +
                                            ar[0]->token.text);
      }

      orign = table->getHandle();
      break;
    case GenericTypeNode::GenericKind::OPTION:
      if (args.size() != 1) {
        Error::diagnostic(g->token, "Option need one type but '" +
                                        to_string(args.size()) + "'");
      }
      orign = table->getOption();
      break;
    case GenericTypeNode::GenericKind::RESULT:
      if (args.size() != 2) {
        Error::diagnostic(g->token, "Result neet two type but '" +
                                        to_string(args.size()) + "'");
      }
      if (args[1]->kind != TypeSymbol::TypeKind::ERROR) {
        Error::diagnostic(g->token, "Result's second type is Error but '" +
                                        args[1]->name);
      }
      break;
    }
    g->resolved = table->GenericInsGetOrCreate(orign, args);
  } else {
    Error::diagnostic(type->token, "unknown type : " + type->token.text);
  }
}
void Resolver::visit(ASTNode *) {}

void Resolver::visit(TraitSig *sig) {
  sig->type->accept(this);
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