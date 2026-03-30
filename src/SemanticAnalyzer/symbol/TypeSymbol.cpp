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
  if (hasSameSig(bucket, symbol)) {
    return false;
  }
  bucket.push_back(symbol);
  return true;
}

bool TypeSymbol::hasSameSig(vector<MethodSymbol *> vec, MethodSymbol *symbol) {
  bool flag = true;

  for (auto &m : vec) {
    if (m->paramTypes.size() != symbol->paramTypes.size()) {
      continue;
    }
    bool flag_ = true;
    for (unsigned int i = 0; i < m->paramTypes.size(); ++i) {
      if (m->paramTypes[i] != symbol->paramTypes[i]) {
        flag_ = false;
        break;
      }
    }
    if (flag_) {
      flag = true;
      break;
    }
  }
  return flag;
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