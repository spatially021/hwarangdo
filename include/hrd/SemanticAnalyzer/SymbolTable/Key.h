#pragma once

#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"

struct ArrayTypeKey {
  TypeSymbol *base;
  llvm::APInt size;
  bool operator==(const ArrayTypeKey &other) const {
    return base == other.base && size == other.size;
  }
};

struct GenericInsKey {
  TypeSymbol *origin;
  std::vector<TypeSymbol *> args;
  bool operator==(const GenericInsKey &other) const {
    return origin == other.origin && args == other.args;
  }
};