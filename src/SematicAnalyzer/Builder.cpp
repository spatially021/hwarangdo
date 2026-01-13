#pragma once

#include "../include/SemanticAnalyzer/Builder.h"

#include <memory>

Builder::Builder(SymbolTable *symbol) : table(symbol) {}

void Builder::visit(LiteralExpr *expr) {}
void Builder::visit(BinaryExpr *expr) {}
void Builder::visit(VarExpr *expr) {}
void Builder::visit(UnaryExpr *expr) {}
void Builder::visit(CallExpr *expr) {}
void Builder::visit(GroupExpr *expr) {}
void Builder::visit(AssignExpr *expr) {}
void Builder::visit(AccessExpr *expr) {}
void Builder::visit(IndexExpr *expr) {}
void Builder::visit(PostfixExpr *expr) {}
void Builder::visit(ArrayAccessExpr *expr) {}
void Builder::visit(TernaryExpr *expr) {}
void Builder::visit(ThisExpr *expr) {}
void Builder::visit(SuperExpr *expr) {}

// Statement Builder::visitor methods
void Builder::visit(ExprStmt *stmt) {}
void Builder::visit(BlockStmt *stmt) {
  ScopeGuard _(*table);
  for (auto s : stmt->statements) {
    s->accept(this);
  }
}
void Builder::visit(IfStmt *stmt) {
  stmt->thenBranch->accept(this);
  if (stmt->elseBranch != nullptr)
    stmt->elseBranch->accept(this);
}
void Builder::visit(ForStmt *stmt) {
  ScopeGuard _(*table);
  stmt->initializer->accept(this);
  stmt->body->accept(this);
}

void Builder::visit(WhileStmt *stmt) { stmt->body->accept(this); }

void Builder::visit(SwitchStmt *stmt) {
  for (auto c : stmt->clauses) {
    c->accept(this);
  }
}
void Builder::visit(Case *stmt) { stmt->body->accept(this); }

void Builder::visit(ReturnStmt *stmt) {}
void Builder::visit(BreakStmt *stmt) {}
void Builder::visit(ContinueStmt *stmt) {}

void Builder::visit(DeclStmt *stmt) { stmt->decl->accept(this); }

void Builder::visit(EmptyStmt *stmt) {}

void Builder::visit(ClassDecl *decl) {
  auto symbol = make_unique<TypeSymbol>();
  symbol->name = decl->name;
  symbol->decl = decl;
  symbol->kind = TypeSymbol::Kind::CLASS;
  symbol->baseName = decl->baseClass;
  auto raw = symbol.get();

  if (!table->add(std::move(symbol))) {
    error(decl->token, "duplicated class name");
  }

  decl->symbol = raw;

  TypeContextGuard _(current, symbol.get());
  ScopeGuard __(*table);

  symbol->memberScope = table->getCurrent();
  for (auto a : decl->body) {
    a->accept(this);
  }
}

void Builder::visit(StructDecl *decl) {
  auto symbol = make_unique<TypeSymbol>();
  symbol->name = decl->name;
  symbol->decl = decl;
  symbol->kind = TypeSymbol::Kind::STRUCT;
  auto raw = symbol.get();

  if (!table->add(std::move(symbol))) {
    error(decl->token, "duplicated struct name");
  }

  decl->symbol = raw;

  TypeContextGuard _(current, symbol.get());
  ScopeGuard __(*table);

  symbol->memberScope = table->getCurrent();
  for (auto a : decl->fields) {
    a->accept(this);
  }
}

void Builder::visit(EnumDecl *decl) {

  auto symbol = make_unique<TypeSymbol>();
  symbol->name = decl->name;
  symbol->decl = decl;
  symbol->kind = TypeSymbol::Kind::ENUM;

  auto raw=symbol.get();

  if (!table->add(std::move(symbol))) {
    error(decl->token, "duplicated enum name");
  }

  decl->symbol = raw;

  TypeContextGuard _(current, symbol.get());

  int ordinal = 0;
  for (auto a : decl->variants) {
    auto v = make_unique<EnumVariantSymbol>();
    v->variant = a.get();
    v->name = a->name;
    v->ordinal = ordinal++;
    if (symbol->variantMap.count(v->name)) {
      error(a->token, "duplicated enum variant name");
    }
    EnumVariantSymbol *raw = v.get();
    symbol->variants.push_back(std::move(v));
    symbol->variantMap.insert({v->name, raw});
  }
}

void Builder::visit(ImplDecl *decl) {
  auto symbol = make_unique<ImplSymbol>();
  symbol->targetName = decl->target;
  symbol->decl = decl;

  ScopeGuard _(*table);
  symbol->member = table->getCurrent();

  for (auto a : decl->methods) {
    a->accept(this);
  }

  table->impls.push_back(std::move(symbol));
  
}

void Builder::visit(TraitDecl *decl) {
  auto symbol = make_unique<TypeSymbol>();
  symbol->name = decl->name;
  symbol->decl = decl;
  symbol->kind = TypeSymbol::Kind::TRAIT;
  auto raw = symbol.get();

  if (!table->add(std::move(symbol))) {
    error(decl->token, "duplicated trait name");
  }

  decl->symbol = raw;

  TypeContextGuard _(current, symbol.get());
  ScopeGuard __(*table);

  symbol->memberScope = table->getCurrent();

  for (auto a : decl->traitSigs) {
    a->accept(this);
  }
}

void Builder::visit(TraitSig *sig) {

  auto symbol = make_unique<ValueSymbol>();
  symbol->name = sig->name;
  symbol->kind = ValueSymbol::Kind::TRAITSIG;
  symbol->node = sig;
  auto raw = symbol.get();

  if (!table->add(std::move(symbol))) {
    error(sig->token, "duplicated trait's method name");
  }

  sig->symbol=raw;

}

void Builder::visit(FuncDecl *decl) {
  auto symbol = make_unique<ValueSymbol>();
  symbol->name = decl->name;
  symbol->kind = ValueSymbol::Kind::METHOD;
  symbol->node = decl;
  auto raw = symbol.get();

  if (!table->add(std::move(symbol))) {
    error(decl->token, "duplicated method name");
  }

  decl->symbol=raw;

  ScopeGuard _(*table);

  for (auto &a : decl->params) {
    auto s = make_unique<ValueSymbol>();
    s->name = a->name;
    s->kind = ValueSymbol::Kind::PARAM;
    s->node = a.get();
    auto r=s.get();
    if (!table->add(std::move(s))) {
      error(a->token, "duplicated parameter name");
    }
    a->symbol=r;
  }

  decl->body->accept(this);
}

void Builder::visit(VarDecl *decl) {
  auto symbol = make_unique<ValueSymbol>();
  symbol->name = decl->name;
  symbol->kind = ValueSymbol::Kind::VAR;
  symbol->node = decl;
  auto raw = symbol.get();

  if (!table->add(std::move(symbol))) {
    error(decl->token, "duplicated var name");
  }
  decl->symbol = raw;
}

void Builder::visit(ArrayDecl *decl) {
  auto symbol = make_unique<ValueSymbol>();
  symbol->name = decl->name;
  symbol->kind = ValueSymbol::Kind::VAR;
  symbol->node = decl;
  auto raw = symbol.get();

  if (!table->add(std::move(symbol))) {
    error(decl->token, "duplicated var name");
  }
  decl->symbol = raw;
}

void Builder::visit(TypeNode *decl) {}
void Builder::visit(ASTNode *node) {}
void Builder::visit(Param *param) {}

void Builder::error(const Token &token, const std::string &message) const {
  string m = "[line ";
  m += std::to_string(token.line);
  m += "] Error at '" + token.text + "': " + message;

  throw runtime_error(m);
}