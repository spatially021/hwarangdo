#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/Scope.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
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
    : origin(o), args(std::move(a)) {
  kind = o->kind;

  name = origin->name + "<";

  for (size_t i = 0; i < args.size(); i++) {
    if (i != 0)
      name += ", ";

    name += args[i]->name;
  }

  name += ">";
}
GenericSymbol::~GenericSymbol() = default;