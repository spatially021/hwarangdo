#include "hrd/SemanticAnalyzer/SymbolTable/ScopeManager.h"
#include "hrd/SemanticAnalyzer/Scope.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/util/Error.h"
#include <memory>

ScopeManager::ScopeManager() {
  topLevel = make_unique<Scope>();
  rootScope = make_unique<Scope>();
  rootScope->id = -1;
  topLevel->id = -1;
}

ScopeManager::~ScopeManager() = default;

bool ScopeManager::addValue(unique_ptr<ValueSymbol> symbol) {
  auto name = symbol->name;
  if (symbol->isRoot) {
    return rootScope->value.emplace(name, std::move(symbol)).second;
  } else {
    return currentScope->value.emplace(name, std::move(symbol)).second;
  }
}

bool ScopeManager::addMethod(unique_ptr<MethodSymbol> symbol) {
  auto raw = symbol.get();
  currentScope->methodOwn.push_back(std::move(symbol));
  currentScope->methodMap[raw->name].push_back(raw);
  return true;
}

bool ScopeManager::addInit(unique_ptr<MethodSymbol> symbol) {
  auto raw = symbol.get();
  currentScope->initOwn.push_back(std::move(symbol));
  currentScope->inits.push_back(raw);
  return true;
}

bool ScopeManager::addOnDestroy(unique_ptr<MethodSymbol> symbol) {
  if (currentScope->onDestroy != nullptr) {
    return false;
  }
  currentScope->onDestroy = std::move(symbol);
  return true;
}

Scope *ScopeManager::current() { return currentScope; }
Scope *ScopeManager::getRootScope() { return rootScope.get(); }
Scope *ScopeManager::getTopLevelScope() { return topLevel.get(); }
int ScopeManager::getScoopID() { return scopeId++; }

void ScopeManager::setCurrentScope(Scope *scope) {
  if (scope == nullptr) {
    Error::internal("set scope is nullptr");
  }
  currentScope = scope;
}

void ScopeManager::enter() {
  auto child = make_unique<Scope>();
  child->parent = current();
  child->id = getScoopID();
  Scope *raw = child.get();
  child->parent->children.push_back(std::move(child));
  setCurrentScope(raw);
}

void ScopeManager::enter(Scope *scope) {

  if (scope == nullptr)
    Error::internal("SymbolTable::enter called with nullptr");

  for (auto &s : current()->children) {
    if (s.get() == scope) {
      setCurrentScope(s.get());
      return;
    }
  }

  Error::internal("Cannot find the given scope as a child of current scope : " +
                  to_string(scope->id));
}

void ScopeManager::exit() {
  assert(currentScope->parent != nullptr);
  setCurrentScope(current()->parent);
}

void ScopeManager::setCurrentToToplevel() {
  setCurrentScope(getTopLevelScope());
}

ValueSymbol *ScopeManager::getValue(const string &name) {
  for (auto s = current(); s != nullptr; s = s->parent) {
    auto it = s->value.find(name);
    if (it != s->value.end())
      return it->second.get();
  }
  return nullptr;
}