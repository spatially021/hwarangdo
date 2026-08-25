#pragma once

#include "hrd/IR/HIR/HIRProgram.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include <memory>
namespace HIRHelper {
void linkSecondPass(HIRProgram *program);
std::unique_ptr<TypeSymbol>
lowerGenericType(HIRProgram *program, HIRSource *source, GenericSymbol *symbol);
} // namespace HIRHelper