#pragma once

#include "Parser.h"
#include "SemanticAnalyzer/Symbol.h"
#include "SemanticAnalyzer/SymbolTable.h"
#include "SemanticAnalyzer/Builder.h"
#include "SemanticAnalyzer/Resolver.h"
#include <memory>
#include <vector>

class SemanticAnalyzer {
public:
  vector<Stmt::Ptr> ast;
  SemanticAnalyzer(std::vector<Stmt::Ptr> s);
  
  void analye();
  
  //util function
  [[noreturn]]
  void error(const Token &token, const std::string &message) const;

private:
  SymbolTable symbolTable;
};