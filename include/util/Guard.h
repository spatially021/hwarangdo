#pragma once
#include "IR/HIR/HIRDecl.h"
#include "IR/HIR/HIRStmt.h"
#include "SemanticAnalyzer/SymbolTable.h"

class TypeContextGuard {
public:
  TypeContextGuard(TypeSymbol *&current, TypeSymbol *next)
      : current_(current), prev_(current) {
    current_ = next;
  }

  ~TypeContextGuard() noexcept { current_ = prev_; }
  TypeContextGuard(const TypeContextGuard &) = delete;
  TypeContextGuard &operator=(const TypeContextGuard &) = delete;

private:
  TypeSymbol *&current_;
  TypeSymbol *prev_ = nullptr;
};

class ScopeGuard {
public:
  ScopeGuard(SymbolTable &t) : table(t) { table.enter(); }
  ScopeGuard(SymbolTable &t, Scope *scope) : table(t) { table.enter(scope); }
  ~ScopeGuard() noexcept { table.exit(); }

  ScopeGuard(const ScopeGuard &) = delete;
  ScopeGuard &operator=(const ScopeGuard &) = delete;

private:
  SymbolTable &table;
};

class BlockGuard {
private:
  HIRBlockStmt *&slot;
  HIRBlockStmt *prev;

public:
  BlockGuard(HIRBlockStmt *&s, HIRBlockStmt *next) : slot(s), prev(s) {
    next->parent = s;
    slot = next;
  }
  ~BlockGuard() { slot = prev; }
};

class BoolGuard {
private:
  bool &slot;
  bool prev;

public:
  BoolGuard(bool &s, bool next) : slot(s), prev(s) { slot = next; }
  ~BoolGuard() { slot = prev; }
};

class MethodGuard {
private:
  HIRMethodDecl *&slot;
  HIRMethodDecl *prev;

public:
  MethodGuard(HIRMethodDecl *&s, HIRMethodDecl *next) : slot(s), prev(s) {
    slot = next;
  }
  ~MethodGuard() { slot = prev; }
};

class TypeGuard {
private:
  HIRTypeDecl *&slot;
  HIRTypeDecl *prve;

public:
  TypeGuard(HIRTypeDecl *&s, HIRTypeDecl *next) : slot(s), prve(s) {
    slot = next;
  }
  ~TypeGuard() { slot = prve; }
};