
#include "hrd/SemanticAnalyzer/symbol/SymbolHelper.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"

bool SymbolHelper::isHandle(TypeSymbol *symbol) {

  if (symbol->kind == TypeSymbol::TypeKind::HANDLE) {
    return true;
  }

  if (auto generic = dynamic_cast<GenericSymbol *>(symbol)) {
    return generic->origin->kind == TypeSymbol::TypeKind::HANDLE;
  }

  return false;
}

TypeSymbol *SymbolHelper::getHandleType(TypeSymbol *t) {
  auto *g = dynamic_cast<GenericSymbol *>(t);
  if (!g || !g->origin || g->origin->kind != TypeSymbol::TypeKind::HANDLE)
    return nullptr;

  return g->args[0];
}
