#pragma once

#include "hrd/SemanticAnalyzer/Scope.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/SourceSpan.h"
class ScopeManager {
public:
  ScopeManager();
  ~ScopeManager();

public:
  // ----- adds -----
  pair<bool, SourceSpan> addValue(unique_ptr<ValueSymbol> valueSymbol);

  // ----- gets -----
  Scope *getRootScope();
  Scope *current();
  Scope *getTopLevelScope();
  int getScoopID();
  ValueSymbol *getValue(const string &name);

  // ----- act -----
  void enter();
  void enter(Scope *scope);
  void exit();
  void setCurrentToToplevel();

private:
  // ----- sets -----
  void setCurrentScope(Scope *scope);

private:
  int scopeId = 1;
  Scope *currentScope = nullptr;
  unique_ptr<Scope> rootScope;
  unique_ptr<Scope> topLevel;
};