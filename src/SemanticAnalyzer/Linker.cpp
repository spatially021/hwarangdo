#include "SemanticAnalyzer/Linker.h"
#include "AST/Decl.h"
#include "AST/Expr.h"
#include "AST/Stmt.h"
#include "SemanticAnalyzer/Scope.h"
#include "SemanticAnalyzer/SymbolTable.h"
#include "SemanticAnalyzer/symbol/MethodSymbol.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "SemanticAnalyzer/symbol/ValueSymbol.h"
#include "util/Error.h"
#include "util/Guard.h"
#include <cassert>
#include <memory>
#include <utility>
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
  expr->receiver->accept(this);
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
  stmt->initializer->accept(this);
  stmt->range->accept(this);
  stmt->body->accept(this);
}
void Linker::visit(WhileStmt *stmt) { stmt->body->accept(this); }
void Linker::visit(SwitchStmt *stmt) {
  for (auto &c : stmt->clauses) {
    c->accept(this);
  }
}
void Linker::visit(Case *c) { c->body->accept(this); }
void Linker::visit(ReturnStmt *stmt) { stmt->value->accept(this); }
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
      Error::diagnostic(decl->token, "has no update method");
    } else if (bucket.size() > 1) {
      Error::diagnostic(decl->token, "not allowed update method overloading");
    }

    symbol->main = bucket[0];
  }

  if (decl->baseClass.has_value()) {
    string s = decl->baseClass.value();
    if (table->isType(s)) {
      auto symbol = table->getType(s);
      symbol->decl->isExtended = true;
      if (symbol->kind != TypeSymbol::TypeKind::CLASS) {
        Error::diagnostic(decl->token, s + " is not class");
      }
      decl->symbol->base = symbol;
    } else {
      Error::diagnostic(decl->token, "unknown parent class '" + s + "'");
    }
  }
  for (auto &t : decl->traits) {
    if (!table->isType(t)) {
      Error::diagnostic(decl->token, "unknown trait '" + t + "'");
    }
    auto symbol = table->getType(t);
    if (symbol->kind != TypeSymbol::TypeKind::TRAIT) {
      Error::diagnostic(decl->token, t + " is not trait");
    }
    decl->symbol->traits.push_back(symbol);
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

  for (auto &t : decl->symbol->traits) {
    auto trait = dynamic_cast<TraitDecl *>(t->decl);
    if (!trait) {
      Error::internal(decl->token,
                      "unmatched decl subClass : " + t->decl->name);
    }
    for (auto &s : trait->traitSigs) {
      auto &bucket = t->traitSigs[s->name];
      if (!Helper::hasSameSig(bucket, s.get())) {
        Error::diagnostic(s->token, "undeclared trait sig : " + s->name);
      }
    }
  }
}

void Linker::visit(StructDecl *decl) {
  ScopeGuard _(*table, decl->symbol->memberScope);
  for (auto &f : decl->fields) {
    f->accept(this);
  }
}
void Linker::visit(EnumDecl *decl) {
  for (auto &v : decl->variants) {
    if (v->payload.has_value()) {
      auto t = v->payload.value().get();
      auto s = table->getType(t);
      if (!s)
        Error::diagnostic(t->token, "unknown type : " + t->token.text);
      t->resolved = s;
      decl->symbol->variantMap[v->name]->payloadType = table->getType(t);
    }
  }
}
void Linker::visit(ImplDecl *decl) {
  auto implIt = table->implMap.find(decl);
  if (implIt == table->implMap.end()) {
    Error::internal(decl->token, "fail to find impl");
  }
  ScopeGuard _(*table, implIt->second->memberScope);
  string s = decl->target;

  if (!table->isType(s)) {
    Error::diagnostic(decl->token, "unknown impl target");
  }
  auto symbol = table->getType(s);
  if (symbol->kind != TypeSymbol::TypeKind::STRUCT) {
    Error::diagnostic(decl->token, s + " is not struct");
  }

  TypeContextGuard __(currentType, symbol);

  ImplSymbol *impl = implIt->second;
  impl->target = symbol;
  if (impl->target == nullptr) {
    Error::internal(decl->token, "impl target is nullptr");
  }
  if (impl->target->memberScope == nullptr) {
    Error::internal(decl->token, "impl target's memberScope is nullptr");
  }

  for (auto &a : decl->LinkedImplMethods) {
    a->accept(this);

    MethodSymbol *methodSymbol = a->methodSymbol;
    if (methodSymbol == nullptr) {
      Error::internal(a->token, "not built methodSymbol : " + a->name);
    }
    if (!symbol->addMethod(methodSymbol)) {
      Error::diagnostic(a->token, "duplicated impl method");
    }
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
  }

  if (decl->isOverride) {
    if (currentType->base->memberScope->methodMap.find(decl->name) ==
        currentType->base->memberScope->methodMap.end()) {
      Error::diagnostic(decl->token, "unkwown override target : " + decl->name);
    }
  }

  if (decl->isFrame) {
    if (!dynamic_cast<MainSymbol *>(currentType)) {
      Error::diagnostic(decl->token, "frame can only in Main class");
    }
    if (decl->name != "update") {
      Error::diagnostic(decl->token, "after frame need method name - update");
    }
  }

  decl->body->accept(this);
}
void Linker::visit(VarDecl *) {}

void Linker::visit(TypeNode *) {}
void Linker::visit(ASTNode *) {}

void Linker::visit(TraitSig *sig) {
  for (auto &p : sig->params) {
    p->accept(this);
  }
  sig->type->accept(this);
}
void Linker::visit(Param *) {}

void Linker::visit(InitDecl *decl) {
  ScopeGuard _(*table, decl->methodSymbol->scope);

  for (auto &p : decl->params) {
    p->accept(this);
    p->symbol->typeSymbol = p->type->resolved;
  }
  decl->body->accept(this);
}
