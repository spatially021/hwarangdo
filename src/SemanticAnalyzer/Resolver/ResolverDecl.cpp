#include "hrd/AST/ASTNode.h"
#include "hrd/AST/Decl.h"
#include "hrd/AST/DeclContext.h"
#include "hrd/AST/Expr.h"
#include "hrd/AST/Stmt.h"
#include "hrd/IR/HIR/HIRType.h"
#include "hrd/SemanticAnalyzer/ResolvedLit.h"
#include "hrd/SemanticAnalyzer/Resolver.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/Symbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/util/Error.h"
#include "hrd/util/Guard.h"
#include "hrd/util/TypeResolver.h"
#include <llvm/ADT/APInt.h>
#include <string>

void Resolver::visit(ClassDecl *decl) {
  if (!decl->symbol) {
    Error::internal(decl->span, "StructDecl symbol not initialized");
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

  if (!Helper::checkImplementTraitSig(decl->symbol)) {
    Error::internal(decl->span, "not implement trait");
  }
}

void Resolver::visit(StructDecl *decl) {
  if (!decl->symbol) {
    Error::internal(decl->span, "StructDecl symbol not initialized");
  }
  ScopeGuard _(*table, decl->symbol->memberScope);
  TypeContextGuard __(currentType, decl->symbol);
  for (auto &a : decl->fields) {
    a->accept(this);
  }

  for (auto &i : decl->inits) {
    i->accept(this);
  }

  if (!Helper::checkImplementTraitSig(decl->symbol)) {
    Error::internal(decl->span, "not implement trait");
  }
}
void Resolver::visit(EnumDecl *) {}
void Resolver::visit(ImplDecl *decl) {
  auto implIt = table->implMap.find(decl);
  if (implIt == table->implMap.end()) {
    Error::internal(decl->span, "fail to find impl");
  }
  ScopeGuard _(*table, implIt->second->memberScope);
  TypeContextGuard __(currentType, implIt->second->target);
  auto prev = currentSelf;
  currentSelf = implIt->second->target->memberScope;
  for (auto &m : decl->LinkedImplMethods) {
    m->accept(this);
    if (!decl->traits.empty() &&
        !Helper::hasSameMethodSig(decl->sigs, m->methodSymbol)) {
      Error::diagnostic(m->span, "not allowed normal method declare here");
    }
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
  auto prev = currentMethod;
  currentMethod = decl->methodSymbol;
  for (auto &p : decl->params) {
    p->accept(this);
  }

  decl->body->accept(this);

  if (decl->returnType.has_value()) {
    auto rt = decl->returnType.value().get();
    rt->accept(this);
    auto type = rt->resolved;
    if (symbol->returns.empty() && table->getType("void") != type) {
      Error::diagnostic(decl->span, "non-void method must have return");
    }
    for (auto r : symbol->returns) {
      if (!isAssignable(type, r->returnType)) {
        Error::diagnostic(r->span, "unmatched return type");
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
          Error::diagnostic(r->span, "unmatched return type");
        }
      }
      symbol->returnType = rt;
    }
  }

  currentMethod = prev;

  if (currentType->base != nullptr) {
    auto it = currentType->base->memberScope->methodMap.find(decl->name);

    if (it == currentType->base->memberScope->methodMap.end()) {
      if (decl->isOverride) {
        Error::diagnostic(decl->span,
                          "unknown override target : " + decl->name);
      }
      return;
    }
    if (Helper::hasSameMethodSig(it->second, decl->methodSymbol)) {
      if (!decl->isOverride) {
        Error::diagnostic(decl->span,
                          "missing 'override' for inherited method : " +
                              decl->name);
      }
    } else if (decl->isOverride) {
      Error::diagnostic(decl->span, "unkwown override target : " + decl->name);
    }
  } else if (decl->isOverride) {
    Error::diagnostic(decl->span, "override not allowed non-inherited classi");
  }

  if (decl->isFrame) {
    if (!dynamic_cast<MainSymbol *>(currentType)) {
      Error::diagnostic(decl->span, "frame can only in Main class");
    }
    if (decl->name != "update") {
      Error::diagnostic(decl->span, "after frame need method name - update");
    }
  }
}

static bool canFieldInit(Expr *init) {
  if (dynamic_cast<LiteralExpr *>(init)) {
    return true;
  }
  if (dynamic_cast<SpawnExpr *>(init)) {
    return true;
  }
  if (auto call = dynamic_cast<CallExpr *>(init)) {
    if (call->callType == CallExpr::CallType::INIT_CALL) {
      return true;
    }
    if (call->callType == CallExpr::CallType::PAYLOAD_CALL) {
      return true;
    }
  }
  if (auto member = dynamic_cast<MemberExpr *>(init)) {
    if (member->resolved->typeSymbol->kind == TypeSymbol::TypeKind::ENUM) {
      return true;
    }
  }

  // TODO: 배열 초기화 방식 추가시 관련 내용 추가하기.

  return false;
}

void Resolver::visit(VarDecl *decl) {

  if (!decl->type->resolved) {
    Error::internal(decl->span, "decl->type->resolved is nullptr");
  }

  if (decl->init) {
    decl->init->accept(this);

    if (decl->symbol->typeSymbol->kind == TypeSymbol::TypeKind::CLASS) {
      if (!dynamic_cast<ViewExpr *>(decl->init.get())) {
        // Error품질 - copy인지, 단순 view사용하지 않은 초기화인지 확인.
        Error::diagnostic(decl->init->span, "using class directly not allowed");
      }
    }

    if (auto lit = dynamic_cast<LiteralExpr *>(decl->init.get())) {
      convertLit(lit, decl->type.get());
      decl->symbol->typeSymbol = decl->type->resolved;
    } else {
      if (dynamic_cast<PrimtiveType *>(decl->type->resolved) &&
          !decl->type->setSize) {
        inferencePrim(decl->type.get(), decl->init->resolvedType);
        decl->symbol->typeSymbol = decl->type->resolved;
      }
      auto [result, kind] =
          canImplicitlyConvert(decl->init->resolvedType, decl->type->resolved);
      if (!result) {
        castFail(kind, decl->init->span);
      }
    }

    if (decl->context == DeclContext::CLASSBODY && decl->init) {
      if (!canFieldInit(decl->init.get())) {
        Error::diagnostic(decl->init->span, "invalid field initializer");
      }
    }
  } else {
    if (decl->symbol->typeSymbol->kind == TypeSymbol::TypeKind::CLASS) {
      Error::diagnostic(decl->span, "observer must be initalize when declare");
    }
  }

  if (!decl->symbol->typeSymbol) {
    Error::internal(decl->span, "typeSymbol is nullptr");
  }
  if (decl->symbol->typeSymbol->kind == TypeSymbol::TypeKind::CLASS) {
    if (decl->init == nullptr) {
      Error::diagnostic(decl->span, "observer variable must be initialized");
    }
    if (currentMethod == nullptr) {
      Error::diagnostic(decl->span,
                        "observer variable must be declared in method");
    }
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
    Error::internal(param->span, "param type is unlinked");
  }
  if (param->defaultValue.has_value()) {
    param->defaultValue.value()->accept(this);
  }
}

void Resolver::visit(InitDecl *decl) {
  auto symbol = decl->methodSymbol;
  for (auto r : symbol->returns) {
    if (r->returnType != table->getBuilt("void")) {
      Error::diagnostic(r->span, "in init cannot declare a return type");
    }
  }

  currentMethod = decl->methodSymbol;
  ScopeGuard _(*table, decl->methodSymbol->scope);
  decl->body->accept(this);
}