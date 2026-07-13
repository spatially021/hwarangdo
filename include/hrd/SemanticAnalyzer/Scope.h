#pragma once

#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::vector;

class Scope {
public:
  Scope();
  ~Scope(); // ✅ 핵심: 선언만

  Scope *parent = nullptr;

  enum class ScopeKind {
    FUNC,
    BLOCK,
    FIELD,
    BUILTIN,
    INIT,
    ONDESTROY
  } scopeKind;

  vector<std::unique_ptr<Scope>> children;

  vector<std::unique_ptr<MethodSymbol>> methodOwn;
  vector<std::unique_ptr<MethodSymbol>> initOwn;

  unordered_map<string, unique_ptr<TypeSymbol>> type;
  unordered_map<string, unique_ptr<ValueSymbol>> value;
  unordered_map<string, vector<MethodSymbol *>> methodMap;
  vector<MethodSymbol *> inits;

  unique_ptr<MethodSymbol> onDestroy = nullptr;

  int id = 0;
  string name = "";
};

class BuiltInScope : public Scope {
public:
  BuiltInScope();
  ~BuiltInScope();
};