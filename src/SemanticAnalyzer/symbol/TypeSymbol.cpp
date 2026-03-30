#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "SemanticAnalyzer/Scope.h"

TypeSymbol::TypeSymbol() {
  type = Symbol::SymbolType::TYPE;
  decl = nullptr;
  base = nullptr;
  memberScope = nullptr;
}
TypeSymbol::~TypeSymbol() = default;

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