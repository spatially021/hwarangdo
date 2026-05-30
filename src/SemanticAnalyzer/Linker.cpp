#include "SemanticAnalyzer/Linker.h"
#include "AST/Decl.h"
#include "AST/Expr.h"
#include "AST/Stmt.h"
#include "IR/HIR/HIRType.h"
#include "SemanticAnalyzer/Scope.h"
#include "SemanticAnalyzer/SymbolTable.h"
#include "SemanticAnalyzer/symbol/MethodSymbol.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "SemanticAnalyzer/symbol/ValueSymbol.h"
#include "util/Error.h"
#include "util/Guard.h"
#include "util/TypeResolver.h"
#include <cassert>
#include <memory>
#include <vector>

Linker::Linker(SymbolTable *t) : table(t) {}

void Linker::visit(LiteralExpr *) {}
void Linker::visit(BinaryExpr *expr) {
  expr->left->accept(this);
  expr->right->accept(this);
}
void Linker::visit(NameExpr *) {}
void Linker::visit(UnaryExpr *expr) { expr->right->accept(this); }
void Linker::visit(CallExpr *expr) {
  if (expr->receiver != nullptr) {
    expr->receiver->accept(this);
  }
  for (auto &a : expr->arguments) {
    a->accept(this);
  }
}
void Linker::visit(AssignExpr *expr) {
  expr->target->accept(this);
  expr->value->accept(this);
}
void Linker::visit(MemberExpr *expr) { expr->object->accept(this); }
void Linker::visit(ArrayAccessExpr *expr) {
  expr->object->accept(this);
  expr->index->accept(this);
}
void Linker::visit(TernaryExpr *expr) {
  expr->conditon->accept(this);
  expr->then->accept(this);
  expr->else_->accept(this);
}
void Linker::visit(ThisExpr *) {}
void Linker::visit(SuperExpr *) {}
void Linker::visit(RootExpr *) {}
void Linker::visit(SelfExpr *) {}
void Linker::visit(CastExpr *) {}
void Linker::visit(BuiltInNameExpr *) {}
void Linker::visit(SpawnExpr *expr) {
  expr->left->accept(this);
  expr->spawnType->accept(this);
}
void Linker::visit(ViewExpr *expr) {
  expr->left->accept(this);
  expr->target->accept(this);
}
void Linker::visit(DestroyExpr *expr) {
  expr->storage->accept(this);
  expr->target->accept(this);
}
void Linker::visit(QuitExpr *) {}
void Linker::visit(DefaultValueExpr *) {}
void Linker::visit(Range *) {}
void Linker::visit(CaseValueExpr *expr) {
  expr->value->accept(this);
  if (expr->arg) {
    expr->arg->accept(this);
  }
}
void Linker::visit(MatchExpr *) {}
// Statement Linker::visitor methods
void Linker::visit(ExprStmt *stmt) { stmt->expr->accept(this); }
void Linker::visit(BlockStmt *stmt) {
  ScopeGuard _(*table, stmt->blockScope);
  for (auto s : stmt->statements) {
    s->accept(this);
  }
}
void Linker::visit(IfStmt *stmt) {
  stmt->thenBranch->accept(this);
  if (stmt->elseBranch != nullptr) {
    stmt->elseBranch->accept(this);
  }
}
void Linker::visit(ForStmt *stmt) {
  ScopeGuard _(*table, stmt->blockScope);
  stmt->initializer->accept(this);
  stmt->range->accept(this);
  stmt->body->accept(this);
}
void Linker::visit(WhileStmt *stmt) { stmt->body->accept(this); }
void Linker::visit(SwitchStmt *stmt) {
  ScopeGuard _(*table, stmt->blockScope);
  for (auto &c : stmt->clauses) {
    c->accept(this);
  }
}
void Linker::visit(Case *c) { c->body->accept(this); }
void Linker::visit(ReturnStmt *stmt) {
  if (stmt->value != nullptr) {
    stmt->value->accept(this);
  }
}
void Linker::visit(ValueTransferStmt *stmt) { stmt->value->accept(this); }
void Linker::visit(BreakStmt *) {}
void Linker::visit(ContinueStmt *) {}
void Linker::visit(DeclStmt *stmt) { stmt->decl->accept(this); }
void Linker::visit(EmptyStmt *) {}

// declare Linker::visitor methods
void Linker::visit(ClassDecl *decl) {

  if (decl->symbol->type == Symbol::SymbolType::MAIN) {
    auto symbol = static_cast<MainSymbol *>(decl->symbol);
    auto &bucket = symbol->memberScope->methodMap["update"];
    if (bucket.size() == 0) {
      Error::diagnostic(decl->span, "missing required 'update' method");
    } else if (bucket.size() > 1) {
      Error::diagnostic(decl->span, "'update' methods cannot be overloaded");
    }
    symbol->main = bucket[0];
  }

  if (decl->baseClass.has_value()) {
    string s = decl->baseClass.value();
    if (table->isType(s)) {
      auto symbol = table->getType(s);
      symbol->decl->isExtended = true;
      if (symbol->kind != TypeSymbol::TypeKind::CLASS) {
        Error::diagnostic(decl->span, "'" + s + "' is not a class type");
      }
      decl->symbol->base = symbol;
    } else {
      Error::diagnostic(decl->span, "unknown base class '" + s + "'");
    }
  }
  for (auto &t : decl->traits) {
    if (!table->isType(t)) {
      Error::diagnostic(decl->span, "unknown trait '" + t + "'");
    }
    auto symbol = table->getType(t);
    if (symbol->kind != TypeSymbol::TypeKind::TRAIT) {
      Error::diagnostic(decl->span, "'" + t + "' is not a trait type");
    }
    if (!decl->symbol->traits.emplace(symbol).second) {
      Error::diagnostic(decl->span,
                        "duplicate trait implementation: " + symbol->name);
    }
  }

  ScopeGuard _(*table, decl->symbol->memberScope);
  TypeContextGuard __(currentType, decl->symbol);

  for (auto &a : decl->fields)
    a->accept(this);
  for (auto &a : decl->methods) {
    a->accept(this);
  }
  for (auto &a : decl->innerDecl) {
    a->accept(this);
  }
}

void Linker::visit(StructDecl *decl) {
  ScopeGuard _(*table, decl->symbol->memberScope);
  for (auto &f : decl->fields) {
    f->accept(this);
  }

  for (auto &i : decl->inits) {
    i->accept(this);
  }
}
void Linker::visit(EnumDecl *decl) {
  for (auto &v : decl->variants) {
    if (v->payload.has_value()) {
      auto t = v->payload.value().get();
      auto s = table->getType(t);
      if (s == nullptr)
        Error::diagnostic(t->span, "unknown type '" + t->type + "'");
      if (s->kind == TypeSymbol::TypeKind::CLASS) {
        Error::diagnostic(
            t->span, "entity types are not allowed in enum variant payloads");
      }
      t->resolved = s;
      decl->symbol->variantMap[v->name]->payloadType = table->getType(t);
    }
  }
}
void Linker::visit(ImplDecl *decl) {
  auto implIt = table->implMap.find(decl);
  if (implIt == table->implMap.end()) {
    Error::internal(decl->span, "fail to find impl");
  }
  ScopeGuard _(*table, implIt->second->memberScope);
  string s = decl->target;

  if (!table->isType(s)) {
    Error::diagnostic(decl->span, "unknown impl target type");
  }
  auto symbol = table->getType(s);
  if (symbol->kind != TypeSymbol::TypeKind::STRUCT) {
    Error::diagnostic(decl->span, "impl target must be a struct type");
  }

  for (auto &c : decl->traits) {
    auto t = table->getType(c);
    if (t->kind != TypeSymbol::TypeKind::TRAIT) {
      Error::diagnostic(decl->span, "impl trait target must be a trait type");
    }
    if (!symbol->traits.emplace(t).second) {
      Error::diagnostic(decl->span,
                        "duplicate trait implementation: " + t->name);
    }
    for (auto &it : t->memberScope->methodMap) {
      for (auto sig : it.second) {
        decl->sigs.push_back(sig);
      }
    }
  }

  TypeContextGuard __(currentType, symbol);

  ImplSymbol *impl = implIt->second;
  impl->target = symbol;
  if (impl->target == nullptr) {
    Error::internal(decl->span, "impl target is nullptr");
  }
  if (impl->target->memberScope == nullptr) {
    Error::internal(decl->span, "impl target's memberScope is nullptr");
  }

  for (auto &a : decl->LinkedImplMethods) {
    MethodSymbol *methodSymbol = a->methodSymbol;
    if (methodSymbol == nullptr) {
      Error::internal(a->span, "not built methodSymbol : " + a->name);
    }
    if (!symbol->addMethod(methodSymbol)) {
      Error::diagnostic(a->span, "duplicate impl method declaration");
    }

    a->accept(this);

    methodSymbol->onwer = symbol;
    methodSymbol->selfScope = symbol->memberScope;
  }
}

void Linker::visit(TraitDecl *decl) {
  ScopeGuard _(*table, decl->symbol->memberScope);
  for (auto &s : decl->traitSigs) {
    s->accept(this);
  }
}

void Linker::visit(FuncDecl *decl) {
  if (decl->returnType.has_value()) {
    decl->returnType->get()->accept(this);
    decl->methodSymbol->returnType = decl->returnType->get()->resolved;
  }

  ScopeGuard _(*table, decl->methodSymbol->scope);
  for (auto &p : decl->params) {
    p->accept(this);
    p->symbol->typeSymbol = p->type->resolved;
    decl->methodSymbol->paramTypes.push_back(p->type->resolved);
  }

  auto it = currentType->memberScope->methodMap.find(decl->methodSymbol->name);
  if (it == currentType->memberScope->methodMap.end()) {
    Error::internal(decl->span, "fail to find method map");
  }

  auto &bucket = it->second;
  auto raw = decl->methodSymbol;
  if (Helper::hasSameMethodSig(bucket, raw)) {
    Error::diagnostic(decl->span,
                      "duplicate method declaration '" + decl->name + "'");
  }

  decl->body->accept(this);
}
void Linker::visit(VarDecl *decl) {
  decl->type->accept(this);
  decl->symbol->typeSymbol = decl->type->resolved;
}

void Linker::visit(TypeNode *type) {
  TypeResolver::resolveTypeNode(type, table);
}
void Linker::visit(ASTNode *) {}

void Linker::visit(TraitSig *sig) {
  for (auto &p : sig->params) {
    p->accept(this);
  }
  sig->type->accept(this);
  sig->symbol->returnType = sig->type->resolved;
}
void Linker::visit(Param *param) {
  param->type->accept(this);
  param->symbol->typeSymbol = param->type->resolved;
}

void Linker::visit(InitDecl *decl) {
  ScopeGuard _(*table, decl->methodSymbol->scope);

  for (auto &p : decl->params) {
    p->accept(this);
    p->symbol->typeSymbol = p->type->resolved;
    decl->methodSymbol->paramTypes.push_back(p->type->resolved);
  }
  decl->body->accept(this);
}
