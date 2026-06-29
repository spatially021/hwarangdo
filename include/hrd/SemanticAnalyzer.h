#pragma once

#include "AST/Program.h"
#include "SemanticAnalyzer/Resolver.h"
#include "SemanticAnalyzer/SymbolTable.h"
#include "SemanticAnalyzer/symbol/RuntimeSymbol.h"
#include "SemanticAnalyzer/symbol/Symbol.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"

enum class LayoutState {
  Unvisited,
  Visiting,
  Done,
};

class SemanticAnalyzer {
public:
  Program *program;
  SemanticAnalyzer(Program *p);
  SymbolTable symbolTable;
  vector<unique_ptr<RuntimeSymbol>> runtimes;

  void build();
  void link();
  void prepareRuntime();
  void addRuntime(std::string namespaceName, std::string name,
                  std::string llvmName, TypeSymbol *returnType,
                  std::vector<TypeSymbol *> params);
  void resolve();

private:
  unordered_map<TypeSymbol *, LayoutState> layoutState;
  void fieldIndexing(TypeSymbol *type);
};
