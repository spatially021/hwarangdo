#include "SemanticAnalyzer/SymbolTable.h"
#include "AST/ASTNode.h"
#include "BuiltInType.h"
#include "SemanticAnalyzer/Scope.h"
#include "SemanticAnalyzer/Symbol.h"
#include "util/Error.h"
#include <cassert>
#include <memory>

using scopePtr = shared_ptr<Scope>;

SymbolTable::SymbolTable() {
  topLevel = make_unique<Scope>();
  current = topLevel.get();
  for (const auto &entry : builtinEntries) {
    std::unique_ptr<TypeSymbol> symbol;

    switch (entry.category) {
    case BuiltinCategory::Int:
      symbol = std::make_unique<IntType>(entry.type);
      break;

    case BuiltinCategory::Float:
      symbol = std::make_unique<FloatType>(entry.type);
      break;

    case BuiltinCategory::Char:
      symbol = std::make_unique<CharType>(entry.type);
      break;

    case BuiltinCategory::String:
      symbol = std::make_unique<StringType>(entry.type);
      break;

    default:
      continue;
    }

    add(std::move(symbol));
  }
  auto symbol = make_unique<TypeSymbol>();
  symbol->name = "void";
  symbol->kind = TypeSymbol::Kind::VOID;
  add(std::move(symbol));

  symbol = make_unique<TypeSymbol>();
  symbol->name = "func";
  symbol->kind = TypeSymbol::Kind::FUNC;
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

bool SymbolTable::isInt(TypeSymbol *symbol) {
  if (symbol->kind == TypeSymbol::Kind::PRIMITIVE) {
    auto p = static_cast<PrimtiveType *>(symbol);
    return p->primtiveKind == PrimtiveType::PrimtiveKind::INT;
  } else
    return false;
}

bool SymbolTable::isBool(TypeSymbol *symbol) {
  return getType("bool") == symbol;
}

TypeSymbol *SymbolTable::getCommonNumbericType(TypeSymbol *left,
                                               TypeSymbol *right) {
  assert(isNumberic(left));
  assert(isNumberic(right));

  if (isInt(left) && isInt(right)) {
    return getType("int");
  }

  if (left == getType("float") && right == getType("float")) {
    return getType("float");
  }
  return nullptr;
}
