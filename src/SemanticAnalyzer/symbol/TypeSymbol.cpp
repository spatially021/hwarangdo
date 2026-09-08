#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/Scope.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SourceSpan.h"
#include "hrd/util/Helper.h"
#include <memory>
#include <utility>
#include <vector>

TypeSymbol::TypeSymbol() {
  type = Symbol::SymbolType::TYPE;
  decl = nullptr;
}
TypeSymbol::~TypeSymbol() = default;

pair<bool, SourceSpan> TypeSymbol::addMethod(unique_ptr<MethodSymbol> symbol) {
  auto raw = symbol.get();
  auto &bucket = methodMap[raw->name];
  if (auto [result, span] = Helper::hasSameSig(bucket, raw); result) {
    return {false, span};
  }
  bucket.push_back(raw);
  methods.push_back(raw);
  methodOwn.push_back(std::move(symbol));
  return {true, {}};
}

pair<bool, SourceSpan> ObjectType::addMethod(unique_ptr<MethodSymbol> symbol,
                                             bool isStatic) {
  auto raw = symbol.get();
  auto &bucket = methodMap[symbol->name];
  if (auto [result, span] = Helper::hasSameSig(bucket, raw); result) {
    return {false, span};
  }
  auto &sBucket = staticMethodMap[symbol->name];
  if (auto [result, span] = Helper::hasSameSig(sBucket, raw); result) {
    return {false, span};
  }
  if (isStatic) {
    sBucket.push_back(raw);
  } else {
    bucket.push_back(raw);
  }
  methods.push_back(raw);
  methodOwn.push_back(std::move(symbol));
  return {true, {}};
}

pair<bool, SourceSpan> ObjectType::addMethod(MethodSymbol *symbol,
                                             bool isStatic) {
  auto &bucket = methodMap[symbol->name];
  if (auto [result, span] = Helper::hasSameSig(bucket, symbol); result) {
    return {false, span};
  }
  auto &sBucket = staticMethodMap[symbol->name];
  if (auto [result, span] = Helper::hasSameSig(sBucket, symbol); result) {
    return {false, span};
  }
  if (isStatic) {
    sBucket.push_back(symbol);
  } else {
    bucket.push_back(symbol);
  }
  methods.push_back(symbol);
  return {true, {}};
}

bool ObjectType::addInit(unique_ptr<MethodSymbol> symbol) {
  auto raw = symbol.get();
  initOwn.push_back(std::move(symbol));
  if (auto [result, span] = Helper::hasSameSig(inits, raw); result) {
    return false;
  }
  inits.push_back(raw);
  return true;
}

MainSymbol::MainSymbol() : ObjectType(TypeKind::CLASS) {
  type = Symbol::SymbolType::MAIN;
}
MainSymbol::~MainSymbol() = default;

GenericSymbol::GenericSymbol(TypeSymbol *o, std::vector<TypeSymbol *> a)
    : origin(o), args(std::move(a)) {
  kind = TypeKind::GENERIC;

  name = origin->name + "<";

  for (size_t i = 0; i < args.size(); i++) {
    if (i != 0)
      name += ", ";

    name += args[i]->name;
  }

  name += ">";
}
GenericSymbol::~GenericSymbol() = default;