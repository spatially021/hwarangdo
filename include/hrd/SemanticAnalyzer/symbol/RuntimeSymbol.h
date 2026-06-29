#pragma once

#include "hrd/SemanticAnalyzer/symbol/Symbol.h"

class RuntimeSymbol : public Symbol {
public:
  std::string namespaceName; // "log"
  std::string name;          // "info"
  TypeSymbol *returnType;
  std::vector<TypeSymbol *> params;
  std::string llvmName; // "hrd_log_info_s8"

private:
  void _anchor() override {}
};