#pragma once

#include "SemanticAnalyzer/Scope.h"
#include <cstddef>
#include <string>

class BuilderDebugger {
public:
  size_t scopeCnt = 0;
  size_t depth = 0;
  std::string ident();
  Scope *toplevel = nullptr;
  void debug();
  void debug(Scope *scope);
  BuilderDebugger(Scope *t);
};