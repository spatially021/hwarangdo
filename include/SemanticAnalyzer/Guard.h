#pragma once

#include "Symbol.h"
#include "SymbolTable.h"

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