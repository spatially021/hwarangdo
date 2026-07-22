#pragma once

#include "AST/Program.h"
#include "SemanticAnalyzer/Resolver.h"
#include "SemanticAnalyzer/SymbolTable.h"
#include "SemanticAnalyzer/symbol/Symbol.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/compiler/CompilerContexts.h"
#include "hrd/util/diagnostic/DiagnosticEngine.h"

enum class LayoutState {
  Unvisited,
  Visiting,
  Done,
};

class SemanticAnalyzer {
public:
  Program *program;
  SemanticAnalyzer(SemanContext &context);
  SymbolTable &symbolTable;
  DiagnosticEngine &engine;

  void build();
  void link();
  void addLog();
  void prepareRuntime();
  void addRuntime(std::string namespaceName, std::string name,
                  std::string llvmName, TypeSymbol *returnType,
                  std::vector<TypeSymbol *> params);
  void resolve();

private:
  unordered_map<TypeSymbol *, LayoutState> layoutState;
  void fieldIndexing(TypeSymbol *type);
};
