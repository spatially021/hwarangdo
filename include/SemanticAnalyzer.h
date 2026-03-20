#pragma once

#include "SemanticAnalyzer/Resolver.h"
#include "SemanticAnalyzer/Scope.h"
#include "SemanticAnalyzer/SymbolTable.h"
#include <vector>

class SemanticAnalyzer {
public:
  vector<StmtPtr> ast;
  SemanticAnalyzer(std::vector<StmtPtr> s);
  SymbolTable symbolTable;

  void build();
  void link();
  void resolve();

private:
};
