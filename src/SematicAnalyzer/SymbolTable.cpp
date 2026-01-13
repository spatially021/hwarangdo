#pragma once

#include "../include/SemanticAnalyzer/SymbolTable.h"
#include <cassert>
#include <memory>

using scopePtr = shared_ptr<Scope>;

SymbolTable::SymbolTable() {
  topLevel = make_unique<Scope>();
  current = topLevel.get();
}

Scope *SymbolTable::getCurrent() { return current; }

void SymbolTable::enter() {
  auto child = make_unique<Scope>();
  child->parent = current;
  Scope *raw = child.get();
  current->children.push_back(std::move(child));
  current = raw;
}

void SymbolTable::exit() {
  assert(current->parent != nullptr);
  current = current->parent;
}

bool SymbolTable::add(unique_ptr<Symbol> symbol) {
  switch (symbol->type) {

  case Symbol::SymbolType::TYPE:
    return addType(unique_ptr<TypeSymbol>(static_cast<TypeSymbol*>(symbol.release())));
  case Symbol::SymbolType::VALUE:
    return addValue(unique_ptr<ValueSymbol>(static_cast<ValueSymbol*>(symbol.release())));
  default:
    return false;
  }
}

bool SymbolTable::addType(unique_ptr<TypeSymbol> symbol) {
  if (current->type.find(symbol->getName()) == current->type.end()) {
    current->type.emplace(symbol->getName(), std::move(symbol));
    return true;
  }
  return false;
}

bool SymbolTable::addValue(unique_ptr<ValueSymbol> symbol) {
  if (current->value.find(symbol->getName()) == current->value.end()) {
    current->value.emplace(symbol->getName(), std::move(symbol));
    return true;
  }
  return false;
}

TypeSymbol * SymbolTable::getType(const string &name) {
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

