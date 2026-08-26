#pragma once

#include "hrd/SemanticAnalyzer/Scope.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
class ScopeManager {
public:
  ScopeManager();
  ~ScopeManager();

public:
  // ----- adds -----
  bool addRoot(unique_ptr<ValueSymbol> symbol);
  bool addValue(unique_ptr<ValueSymbol> valueSymbol);
  bool addMethod(unique_ptr<MethodSymbol> methodSymbol);
  bool addInit(unique_ptr<MethodSymbol> methodSymbol);
  bool addOnDestroy(unique_ptr<MethodSymbol> methodSymbol);

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