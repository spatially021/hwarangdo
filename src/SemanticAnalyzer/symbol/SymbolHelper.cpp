
#include "hrd/SemanticAnalyzer/symbol/SymbolHelper.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"

bool SymbolHelper::isHandle(TypeSymbol *symbol) {

  if (symbol->kind == TypeKind::HANDLE) {
    return true;
  }

  if (auto generic = dynamic_cast<GenericSymbol *>(symbol)) {
    return generic->origin->kind == TypeKind::HANDLE;
  }

  return false;
}

TypeSymbol *SymbolHelper::getHandleType(TypeSymbol *t) {
  auto *g = dynamic_cast<GenericSymbol *>(t);
  if (!g || !g->origin || g->origin->kind != TypeKind::HANDLE)
    return nullptr;

  return g->args[0];
}

bool SymbolHelper::isNumberic(TypeSymbol *symbol) {
  return isa<IntType>(symbol) || isa<FloatType>(symbol);
}

bool SymbolHelper::isSigned(TypeSymbol *symbol) {
  {
    if (!isNumberic(symbol)) {
      return false;
    }
    if (auto i = cast<IntType>(symbol)) {
      return i->isSigned;
    }

    return false;
  }
}