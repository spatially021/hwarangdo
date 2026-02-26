#pragma once

#include "Scope.h"
#include "Symbol.h"
#include <memory>

class SymbolTable {
  using scopePtr = shared_ptr<Scope>;
  using str = const string &;

public:
  SymbolTable();
  vector<unique_ptr<ImplSymbol>> impls;
  unordered_map<Decl *, ImplSymbol *> implMap;

  bool add(unique_ptr<Symbol> symbol);

  void enter();
  void enter(Scope *scope);
  void exit();

  Symbol resolve(str name);

  TypeSymbol *getType(str name);
  MethodSymbol *getMethod(str name);

  bool isType(str name);
  bool isValue(str name);
  bool isMethod(str name);

  bool isNumberic(TypeSymbol *symbol);
  bool isInt(TypeSymbol *symbol);
  bool isBool(TypeSymbol *symbol);

  TypeSymbol *getCommonNumbericType(TypeSymbol *left, TypeSymbol *right);

  Scope *getCurrent();

  TypeSymbol *getUnknown();

  void addImpl(unique_ptr<ImplSymbol>);

private:
  Scope *current = nullptr;
  unique_ptr<TypeSymbol> unknown;
  TypeSymbol *currentType = nullptr;
  unique_ptr<Scope> topLevel;
  bool addValue(unique_ptr<ValueSymbol> symbol);
  bool addType(unique_ptr<TypeSymbol> symbol);
  bool addMethod(unique_ptr<MethodSymbol> symbol);
  ValueSymbol *getValue(str name);
  friend class Resolver;
  int scopeId = 1;
};