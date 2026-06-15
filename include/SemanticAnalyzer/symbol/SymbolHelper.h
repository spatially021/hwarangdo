#pragma once

#include "SemanticAnalyzer/symbol/TypeSymbol.h"
namespace SymbolHelper {
bool isHandle(TypeSymbol *symbol);
TypeSymbol *getHandleType(TypeSymbol *symbol);
} // namespace SymbolHelper