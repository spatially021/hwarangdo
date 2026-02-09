#include "SemanticAnalyzer/SymbolTable.h"
#include "SemanticAnalyzer/Scope.h"
#include "util/Error.h"
#include <cassert>

using scopePtr = shared_ptr<Scope>;

SymbolTable::SymbolTable() {
  topLevel = make_unique<Scope>();
  current = topLevel.get();

  auto symbol = make_unique<TypeSymbol>();
  symbol->name = "int";
  symbol->kind = TypeSymbol::Kind::BUILTIN;
  add(std::move(symbol));

  symbol = make_unique<TypeSymbol>();
  symbol->name = "float";
  symbol->kind = TypeSymbol::Kind::BUILTIN;
  add(std::move(symbol));

  symbol = make_unique<TypeSymbol>();
  symbol->name = "fixed";
  symbol->kind = TypeSymbol::Kind::BUILTIN;
  add(std::move(symbol));

  symbol = make_unique<TypeSymbol>();
  symbol->name = "char";
  symbol->kind = TypeSymbol::Kind::BUILTIN;
  add(std::move(symbol));

  symbol = make_unique<TypeSymbol>();
  symbol->name = "string";
  symbol->kind = TypeSymbol::Kind::BUILTIN;
  add(std::move(symbol));

  symbol = make_unique<TypeSymbol>();
  symbol->name = "bool";
  symbol->kind = TypeSymbol::Kind::BUILTIN;
  add(std::move(symbol));

  symbol = make_unique<TypeSymbol>();
  symbol->name = "void";
  symbol->kind = TypeSymbol::Kind::BUILTIN;
  add(std::move(symbol));

  symbol = make_unique<TypeSymbol>();
  symbol->name = "func";
  symbol->kind = TypeSymbol::Kind::BUILTIN;
  add(std::move(symbol));

  unknown = make_unique<TypeSymbol>();
  unknown->kind = TypeSymbol::Kind::UNKNOWN;
}

Scope *SymbolTable::getCurrent() { return current; }

bool SymbolTable::isNumberic(TypeSymbol *symbol) {
  return symbol == getType("int") || symbol == getType("float") ||
         symbol == getType("fixed");
}

void SymbolTable::enter() {
  auto child = make_unique<Scope>();
  child->parent = current;
  child->id = scopeId;
  scopeId++;
  Scope *raw = child.get();
  current->children.push_back(std::move(child));
  current = raw;
}

void SymbolTable::enter(Scope *scope) {

  if (!scope)
    Error::internal("SymbolTable::enter called with nullptr");
  if (!current)
    Error::internal("SymbolTable::current is null");

  for (auto &s : current->children) {
    if (s.get() == scope) {
      current = s.get();
      return;
    }
  }

  Error::internal("Cannot find the given scope as a child of current scope");
}

void SymbolTable::exit() {
  assert(current->parent != nullptr);
  current = current->parent;
}

bool SymbolTable::add(unique_ptr<Symbol> symbol) {
  switch (symbol->type) {

  case Symbol::SymbolType::TYPE:
    return addType(
        unique_ptr<TypeSymbol>(static_cast<TypeSymbol *>(symbol.release())));
  case Symbol::SymbolType::VALUE:
    return addValue(
        unique_ptr<ValueSymbol>(static_cast<ValueSymbol *>(symbol.release())));
  case Symbol::SymbolType::METHOD:
    return addMethod(unique_ptr<MethodSymbol>(
        static_cast<MethodSymbol *>(symbol.release())));
  default:
    return false;
  }
}

bool SymbolTable::addType(unique_ptr<TypeSymbol> symbol) {
  return current->type.emplace(symbol->name, std::move(symbol)).second;
}

bool SymbolTable::addValue(unique_ptr<ValueSymbol> symbol) {
  return current->value.emplace(symbol->name, std::move(symbol)).second;
}

bool SymbolTable::addMethod(unique_ptr<MethodSymbol> symbol) {
  return current->method.emplace(symbol->name, std::move(symbol)).second;
}

TypeSymbol *SymbolTable::getType(const string &name) {
  for (auto s = current; s != nullptr; s = s->parent) {
    if (s->type.find(name) != s->type.end())
      return s->type.find(name)->second.get();
  }
  return nullptr;
}

ValueSymbol *SymbolTable::getValue(const string &name) {
  for (auto s = current; s != nullptr; s = s->parent) {
    if (s->value.find(name) != s->value.end())
      return s->value.find(name)->second.get();
  }
  return nullptr;
}

bool SymbolTable::isType(const string &name) {
  return getType(name) != nullptr;
}

bool SymbolTable::isValue(const string &name) {
  return getValue(name) != nullptr;
}

TypeSymbol *SymbolTable::getUnknown() { return this->unknown.get(); }
