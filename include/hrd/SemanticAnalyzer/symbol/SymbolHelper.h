#pragma once

#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
namespace SymbolHelper {
bool isHandle(TypeSymbol *symbol);
TypeSymbol *getHandleType(TypeSymbol *symbol);
bool isNumberic(TypeSymbol *symbol);
bool isSigned(TypeSymbol *symbol);

} // namespace SymbolHelper