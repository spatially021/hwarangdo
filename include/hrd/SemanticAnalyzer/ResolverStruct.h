#pragma once

#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
struct ResolvedMethod {
  MethodSymbol *method = nullptr;
  TypeSymbol *returnType = nullptr;
  unordered_map<GenericParamSymbol *, TypeSymbol *> substitution;
};