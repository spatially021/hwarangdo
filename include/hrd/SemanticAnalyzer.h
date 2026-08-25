#pragma once

#include "AST/Program.h"
#include "SemanticAnalyzer/Resolver.h"
#include "SemanticAnalyzer/SymbolTable/SymbolTable.h"
#include "SemanticAnalyzer/symbol/Symbol.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/Recover/SementicRecover.h"
#include "hrd/compiler/CompilerContexts.h"
#include "hrd/diagnostic/DiagnosticEngine.h"

enum class LayoutState {
  Unvisited,
  Visiting,
  Done,
};

class SemanticAnalyzer {
public:
  Program *program;
  SemanticAnalyzer(SemanContext &context);
  SymbolTable &table;
  DiagnosticEngine &engine;
  bool isCompile;
  SemanticAnalyzerRecover recover;
  void build();
  void import();
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
