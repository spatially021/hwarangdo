#pragma once

#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"

struct IRScope {
  IRScope *parent = nullptr;
  vector<ValueSymbol *> locals;
  int depth = 0;
  IRScope(IRScope *p, int d) : parent(p), depth(d) {}
};