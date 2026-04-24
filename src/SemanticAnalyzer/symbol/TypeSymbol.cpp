#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "SemanticAnalyzer/Scope.h"
#include "SemanticAnalyzer/symbol/MethodSymbol.h"
#include <vector>

TypeSymbol::TypeSymbol() {
  type = Symbol::SymbolType::TYPE;
  decl = nullptr;
  base = nullptr;
  memberScope = nullptr;
}
TypeSymbol::~TypeSymbol() = default;

bool TypeSymbol::addMethod(MethodSymbol *symbol) {
  auto &bucket = memberScope->methodMap[symbol->name];
  if (Helper::hasSameSig(bucket, symbol)) {
    return false;
  }
  bucket.push_back(symbol);
  return true;
}

MainSymbol::MainSymbol() {
  type = Symbol::SymbolType::MAIN;
  kind = TypeSymbol::TypeKind::CLASS;
  rootScope = make_unique<Scope>();
}
MainSymbol::~MainSymbol() = default;
GenericSymbol::GenericSymbol(TypeSymbol *o, std::vector<TypeSymbol *> a)
    : origin(o), args(a) {
  kind = o->kind;
};
GenericSymbol::~GenericSymbol() = default;