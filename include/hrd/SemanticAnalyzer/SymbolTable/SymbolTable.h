#pragma once

#include "hrd/AST/ASTNode.h"
#include "hrd/BuiltInType.h"
#include "hrd/SemanticAnalyzer/Module.h"
#include "hrd/SemanticAnalyzer/Scope.h"
#include "hrd/SemanticAnalyzer/SymbolTable/ScopeManager.h"
#include "hrd/SemanticAnalyzer/SymbolTable/SymbolRegistry.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/Symbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SourceSpan.h"

#include <llvm/ADT/APInt.h>
#include <memory>

struct Result {
  bool success = true;
  enum ErrorType {
    DUPLICATED,
    RESERVED,
    UNKNOWN_SYMBOL,
    NONE,
  } errorType = NONE;
  SourceSpan span;
};

class SymbolTable {
  using scopePtr = shared_ptr<Scope>;
  using str = const string &;

public:
  ScopeManager scopeManger = ScopeManager();
  SymbolRegistry registry = SymbolRegistry();

public:
  SymbolTable();
  ~SymbolTable();

  Module *moudle = nullptr;
  MainSymbol *main = nullptr;
  Result add(unique_ptr<Symbol> symbol);

  TypeSymbol *getType(TypeNode *node);
  TypeSymbol *getType(str node);
  TypeSymbol *getBuilt(BuiltInType type);

  inline TypeSymbol *getBuiltName() { return registry.getBuilt("@built"); }
  inline TypeSymbol *getDefaultV() { return registry.getBuilt("@default"); }

  inline bool isType(str name) { return registry.getType(name) != nullptr; }
  inline bool isValue(str name) {
    return scopeManger.getValue(name) != nullptr;
  }

  TypeSymbol *getCommonNumbericType(TypeSymbol *left, TypeSymbol *right);
};
