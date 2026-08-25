#include "hrd/SemanticAnalyzer/SymbolTable/SymbolRegistry.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"

HandleSymbol *SymbolRegistry::getHandle() {
  return dynamic_cast<HandleSymbol *>(builtIn.at("Handle"));
}

ResultSymbol *SymbolRegistry::getResult() {
  return dynamic_cast<ResultSymbol *>(builtIn.at("Result"));
}

OptionSymbol *SymbolRegistry::getOption() {
  return dynamic_cast<OptionSymbol *>(builtIn.at("Option"));
}

TypeSymbol *SymbolRegistry::getBool() { return builtIn.at("bool"); }