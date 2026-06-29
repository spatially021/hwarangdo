#pragma once

#include "hrd/SemanticAnalyzer/Scope.h"
#include <cstddef>
#include <string>

class ResolverDebugger {
public:
  std::size_t depth = 0;
  Scope *toplevel = nullptr;
  std::string ident();
  void debug();
  void debug(Scope *scope);
  ResolverDebugger(Scope *scope);
};