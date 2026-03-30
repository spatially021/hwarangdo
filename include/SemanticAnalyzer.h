#pragma once

#include "AST/Program.h"
#include "SemanticAnalyzer/Resolver.h"
#include "SemanticAnalyzer/SymbolTable.h"

class SemanticAnalyzer {
public:
  Program *program;
  SemanticAnalyzer(Program *p);
  SymbolTable symbolTable;

  void build();
  void link();
  void resolve();

private:
};
