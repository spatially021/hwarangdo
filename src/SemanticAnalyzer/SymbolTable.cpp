#include "SemanticAnalyzer/SymbolTable.h"
#include "AST/ASTNode.h"
#include "BuiltInType.h"
#include "SemanticAnalyzer/Scope.h"
#include "SemanticAnalyzer/symbol/MethodSymbol.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "SemanticAnalyzer/symbol/ValueSymbol.h"
#include "util/Error.h"
#include <cassert>
#include <llvm/ADT/APInt.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using scopePtr = shared_ptr<Scope>;

SymbolTable::SymbolTable() {
  topLevel = make_unique<Scope>();
  builtIn = make_unique<BuiltInScope>();
  rootScope = make_unique<Scope>();
  rootScope->id = -1;
  topLevel->parent = builtIn.get();
  current = builtIn.get();
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
    case BuiltinCategory::Bool:
      symbol = std::make_unique<BoolType>();
      break;
    default:
      continue;
    }
    if (!symbol) {
      Error::internal("failed to create builtin type symbol");
    }
    if (symbol->name.empty()) {
      Error::internal("builtin type symbol has empty name");
    }
    add(std::move(symbol));
  }
  auto symbol = make_unique<TypeSymbol>();
  symbol->name = "void";
  symbol->kind = TypeSymbol::TypeKind::VOID;
  add(std::move(symbol));

  symbol = make_unique<TypeSymbol>();
  symbol->name = "func";
  symbol->kind = TypeSymbol::TypeKind::FUNC;
  add(std::move(symbol));

  symbol = make_unique<HandleSymbol>();
  symbol->name = "Handle";
  symbol->kind = TypeSymbol::TypeKind::HANDLE;
  add(std::move(symbol));

  symbol = make_unique<ResultSymbol>();
  symbol->name = "Result";
  symbol->kind = TypeSymbol::TypeKind::RESULT;
  add(std::move(symbol));

  symbol = make_unique<OptionSymbol>();
  symbol->name = "Option";
  symbol->kind = TypeSymbol::TypeKind::OPTION;
  add(std::move(symbol));

  symbol = make_unique<ErrorType>();
  symbol->name = "Error";
  symbol->kind = TypeSymbol::TypeKind::ERROR;
  add(std::move(symbol));

  symbol = make_unique<TypeSymbol>();
  symbol->name = "@built";
  symbol->kind = TypeSymbol::TypeKind::BUILTIN;
  add(std::move(symbol));

  symbol = make_unique<TypeSymbol>();
  symbol->name = "@default";
  symbol->kind = TypeSymbol::TypeKind::DEFAULT_VALUE;
  add(std::move(symbol));

  current = topLevel.get();
  topLevel->id = -1;
  builtIn->id = -2;
}

Scope *SymbolTable::getCurrent() { return current; }

bool SymbolTable::isNumberic(TypeSymbol *symbol) {
  return isInt(symbol) || isFloat(symbol) || isFixed(symbol);
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

  Error::internal("Cannot find the given scope as a child of current scope : " +
                  to_string(scope->id));
}

void SymbolTable::exit() {
  assert(current->parent != nullptr);
  current = current->parent;
}

SymbolTable::Result SymbolTable::add(unique_ptr<Symbol> symbol) {
  assert(symbol != nullptr);
  switch (symbol->type) {
  case Symbol::SymbolType::TYPE: {
    std::unique_ptr<TypeSymbol> s(static_cast<TypeSymbol *>(symbol.release()));

    if (getType(s->name) != nullptr) {
      if (s->isReserved)
        return {false, Result::RESERVED};
      return {false, Result::DUPLICATED};
    }

    bool ok = addType(std::move(s));
    return {ok, ok ? Result::NONE : Result::DUPLICATED};
  }
  case Symbol::SymbolType::VALUE: {
    bool b = addValue(
        unique_ptr<ValueSymbol>(static_cast<ValueSymbol *>(symbol.release())));
    return SymbolTable::Result(
        {b, (b ? SymbolTable::Result::NONE : SymbolTable::Result::DUPLICATED)});
  }

  case Symbol::SymbolType::METHOD: {
    bool b = addMethod(unique_ptr<MethodSymbol>(
        static_cast<MethodSymbol *>(symbol.release())));
    return SymbolTable::Result(
        {b, (b ? SymbolTable::Result::NONE : SymbolTable::Result::DUPLICATED)});
  }

  case Symbol::SymbolType::MAIN: {
    bool b = addType(
        unique_ptr<MainSymbol>(static_cast<MainSymbol *>(symbol.release())));
    return SymbolTable::Result(
        {b, (b ? SymbolTable::Result::NONE : SymbolTable::Result::DUPLICATED)});
  }

  default:
    return SymbolTable::Result({false, SymbolTable::Result::UNKNOWN_SYMBOL});
  }
}

bool SymbolTable::addType(unique_ptr<TypeSymbol> symbol) {
  return current->type.emplace(symbol->name, std::move(symbol)).second;
}

bool SymbolTable::addValue(unique_ptr<ValueSymbol> symbol) {
  if (symbol->isRoot) {
    return rootScope->value.emplace(symbol->name, std::move(symbol)).second;
  } else {
    return current->value.emplace(symbol->name, std::move(symbol)).second;
  }
}

bool SymbolTable::addMethod(unique_ptr<MethodSymbol> symbol) {
  auto &bucket = current->methodMap[symbol->name];
  MethodSymbol *raw = symbol.get();

  if (Helper::hasSameSig(bucket, raw)) {
    return false;
  }

  current->methodOwn.push_back(std::move(symbol));
  bucket.push_back(raw);
  return true;
}

TypeSymbol *SymbolTable::getType(str name) {
  for (auto s = current; s != nullptr; s = s->parent) {
    if (s->type.find(name) != s->type.end())
      return s->type.find(name)->second.get();
  }

  return nullptr;
}

TypeSymbol *SymbolTable::getType(TypeNode *node) {
  if (node->kind == NKind::BUILT_IN_TYPE) {
    auto b = static_cast<BuiltinTypeNode *>(node);
    switch (b->type) {
    case BuiltInType::I8:
      return getBuilt("i8");
    case BuiltInType::I16:
      return getBuilt("i16");
    case BuiltInType::I32:
      return getBuilt("i32");
    case BuiltInType::I64:
      return getBuilt("i64");
    case BuiltInType::I128:
      return getBuilt("i128");
    case BuiltInType::U8:
      return getBuilt("u8");
    case BuiltInType::U16:
      return getBuilt("u16");
    case BuiltInType::U32:
      return getBuilt("u32");
    case BuiltInType::U64:
      return getBuilt("u64");
    case BuiltInType::U128:
      return getBuilt("u128");
    case BuiltInType::F16:
      return getBuilt("f16");
    case BuiltInType::F32:
      return getBuilt("f32");
    case BuiltInType::F64:
      return getBuilt("f64");
    case BuiltInType::F128:
      return getBuilt("f128");
    case BuiltInType::C8:
      return getBuilt("c8");
    case BuiltInType::C16:
      return getBuilt("c16");
    case BuiltInType::C32:
      return getBuilt("c32");
    case BuiltInType::B:
      return getBuilt("bool");
    case BuiltInType::FI:
      return getBuilt("fixed");
    case BuiltInType::S8:
      return getBuilt("s8");
    case BuiltInType::S16:
      return getBuilt("s16");
    case BuiltInType::S32:
      return getBuilt("s32");
    }
  } else {
    return getType(node->type);
  }
  return nullptr;
}

ValueSymbol *SymbolTable::getValue(str name) {
  for (auto s = current; s != nullptr; s = s->parent) {
    if (s->value.find(name) != s->value.end())
      return s->value.find(name)->second.get();
  }
  return nullptr;
}

HandleSymbol *SymbolTable::getHandle() {
  auto it = builtIn.get()->type.find("Handle");
  if (it == builtIn.get()->type.end()) {
    Error::internal("'Handle' not initated");
  }
  return dynamic_cast<HandleSymbol *>(it->second.get());
}

ResultSymbol *SymbolTable::getResult() {
  auto it = builtIn.get()->type.find("Result");
  if (it == builtIn.get()->type.end()) {
    Error::internal("'Result' not initated");
  }
  return dynamic_cast<ResultSymbol *>(it->second.get());
}

OptionSymbol *SymbolTable::getOption() {
  auto it = builtIn.get()->type.find("Option");
  if (it == builtIn.get()->type.end()) {
    Error::internal("'Option' not initated");
  }
  return dynamic_cast<OptionSymbol *>(it->second.get());
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

TypeSymbol *SymbolTable::getBuilt(str name) {
  auto it = builtIn.get()->type.find(name);
  if (it == builtIn.get()->type.end()) {
    Error::internal(name + " is not initated");
  }
  return it->second.get();
}

GenericSymbol *
SymbolTable::GenericInsGetOrCreate(TypeSymbol *origin,
                                   std::vector<TypeSymbol *> args) {
  auto key = GenericInsKey({origin, args});
  auto it = genericInsSMap.find(key);
  if (it == genericInsSMap.end()) {
    auto ins = make_unique<GenericSymbol>(origin, args);
    auto raw = ins.get();
    genericInsStorage.push_back(std::move(ins));
    it = genericInsSMap.emplace(key, raw).first;
  }
  return it->second;
}

SymbolTable::Result SymbolTable::addInit(unique_ptr<MethodSymbol> initMethod) {
  auto [it, inserted] = current->inits.emplace("init", std::move(initMethod));
  return {inserted, inserted ? Result::NONE : Result::DUPLICATED};
  // TODO: 메서드 overloading추가하면서 오버로딩 규칙 추가
}

ArrayTypeSymbol *SymbolTable::arrayTypeGetOrCreate(TypeSymbol *base,
                                                   llvm::APInt size) {
  auto key = ArrayTypeKey({base, size});
  auto it = arrayTypeMap.find(key);
  if (it == arrayTypeMap.end()) {
    auto type = make_unique<ArrayTypeSymbol>(base, size);
    auto raw = type.get();
    arrayTypeStorage.push_back(std::move(type));
    it = arrayTypeMap.emplace(key, raw).first;
  }
  return it->second;
}