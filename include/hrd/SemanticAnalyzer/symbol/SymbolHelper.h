#pragma once

#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
namespace SymbolHelper {
bool isHandle(TypeSymbol *symbol);
TypeSymbol *getHandleType(TypeSymbol *symbol);
} // namespace SymbolHelper