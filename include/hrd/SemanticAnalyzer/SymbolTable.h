#pragma once

#include "Scope.h"
#include "hrd/AST/ASTNode.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/RuntimeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "symbol/Symbol.h"
#include "symbol/TypeSymbol.h"
#include <cstddef>
#include <functional>
#include <llvm/ADT/APInt.h>
#include <memory>
#include <utility>
#include <vector>

struct RuntimeNamespace {
  string name;
  unordered_map<string, vector<RuntimeSymbol *>> functions;
  RuntimeNamespace(string n) : name(n) {}
};

struct ArrayTypeKey {
  TypeSymbol *base;
  llvm::APInt size;
  bool operator==(const ArrayTypeKey &other) const {
    return base == other.base && size == other.size;
  }
};

#include "llvm/ADT/Hashing.h"

struct ArrayTypeHash {
  size_t operator()(const ArrayTypeKey &k) const {
    using llvm::hash_value;
    return llvm::hash_combine(k.base, hash_value(k.size));
  }
};

struct GenericInsKey {
  TypeSymbol *origin;
  std::vector<TypeSymbol *> args;
  bool operator==(const GenericInsKey &other) const {
    return origin == other.origin && args == other.args;
  }
};
struct GenericInsHash {
  size_t operator()(const GenericInsKey &k) const {
    size_t h = std::hash<TypeSymbol *>()(k.origin);

    for (auto *arg : k.args) {
      h ^= std::hash<TypeSymbol *>()(arg) + 0x9e3779b9 + (h << 6) + (h >> 2);
    }

    return h;
  }
};
class SymbolTable {
  using scopePtr = shared_ptr<Scope>;
  using str = const string &;

public:
  struct Result {
    bool success = true;
    enum ErrorType {
      DUPLICATED,
      RESERVED,
      UNKNOWN_SYMBOL,
      NONE,
    } errorType = NONE;
  };

  SymbolTable();
  vector<unique_ptr<ImplSymbol>> impls;
  unordered_map<Decl *, ImplSymbol *> implMap;
  MainSymbol *main = nullptr;
  unique_ptr<Scope> rootScope;
  vector<unique_ptr<GenericSymbol>> genericInsStorage;
  unordered_map<GenericInsKey, GenericSymbol *, GenericInsHash> genericInsSMap;
  vector<unique_ptr<RuntimeSymbol>> runtimes;

  vector<unique_ptr<ArrayTypeSymbol>> arrayTypeStorage;
  unordered_map<ArrayTypeKey, ArrayTypeSymbol *, ArrayTypeHash> arrayTypeMap;

  vector<TypeSymbol *> types;

  unordered_map<string, RuntimeNamespace> runtimeMap;

  Result add(unique_ptr<Symbol> symbol);
  bool addInit(unique_ptr<MethodSymbol> initMethod);
  bool addOnDestroy(unique_ptr<MethodSymbol> onDestroy);

  void enter();
  void enter(Scope *scope);
  void exit();

  Symbol resolve(str name);

  TypeSymbol *getType(TypeNode *node);
  TypeSymbol *getType(str name);
  TypeSymbol *getBuilt(str name);

  inline TypeSymbol *getBuiltName() { return getBuilt("@built"); }
  inline TypeSymbol *getDefaultV() { return getBuilt("@default"); }

  MethodSymbol *getMethod(str name);
  HandleSymbol *getHandle();
  OptionSymbol *getOption();
  ResultSymbol *getResult();
  TypeSymbol *getBool();

  GenericSymbol *GenericInsGetOrCreate(TypeSymbol *origin,
                                       std::vector<TypeSymbol *> args);
  ArrayTypeSymbol *arrayTypeGetOrCreate(TypeSymbol *base, llvm::APInt size);

  inline bool isType(str name) { return getType(name) != nullptr; }
  inline bool isValue(str name) { return getValue(name) != nullptr; }
  bool isMethod(str name);

  bool isNumberic(TypeSymbol *symbol);
  bool isSigned(TypeSymbol *symbol);
  inline bool isInt(TypeSymbol *symbol) {
    return symbol->kind == TypeSymbol::TypeKind::PRIMITIVE
               ? (static_cast<PrimtiveType *>(symbol)->builtinCategory ==
                  BuiltinCategory::Int)
               : (false);
  }
  inline bool isFloat(TypeSymbol *symbol) {
    return symbol->kind == TypeSymbol::TypeKind::PRIMITIVE
               ? (static_cast<PrimtiveType *>(symbol)->builtinCategory ==
                  BuiltinCategory::Float)
               : (false);
  }
  inline bool isFixed(TypeSymbol *symbol) {
    return symbol->kind == TypeSymbol::TypeKind::PRIMITIVE
               ? (static_cast<PrimtiveType *>(symbol)->builtinCategory ==
                  BuiltinCategory::Fixed)
               : (false);
  }
  inline bool isBool(TypeSymbol *symbol) { return getType("bool") == symbol; }
  inline bool isString(TypeSymbol *symbol) {
    return symbol->kind == TypeSymbol::TypeKind::PRIMITIVE
               ? (static_cast<PrimtiveType *>(symbol)->builtinCategory ==
                  BuiltinCategory::String)
               : (false);
  }

  inline bool isChar(TypeSymbol *symbol) {
    return symbol->kind == TypeSymbol::TypeKind::PRIMITIVE
               ? (static_cast<PrimtiveType *>(symbol)->builtinCategory ==
                  BuiltinCategory::Char)
               : (false);
  }

  TypeSymbol *getCommonNumbericType(TypeSymbol *left, TypeSymbol *right);

  Scope *getCurrent();

  void addImpl(unique_ptr<ImplSymbol>);
  void addTemp(unique_ptr<ValueSymbol> symbol) {
    tmps.push_back(std::move(symbol));
  }
  ValueSymbol *makePayloadValue(TypeSymbol *type);

private:
  Scope *current = nullptr;
  unique_ptr<TypeSymbol> unknown;
  TypeSymbol *currentType = nullptr;
  unique_ptr<Scope> topLevel;
  unique_ptr<BuiltInScope> builtIn;
  vector<unique_ptr<ValueSymbol>> tmps;
  vector<unique_ptr<ValueSymbol>> payloadSymbols;
  vector<unique_ptr<ValueSymbol>> selfSymbols;

  bool addValue(unique_ptr<ValueSymbol> symbol);
  bool addType(unique_ptr<TypeSymbol> symbol);
  bool addMethod(unique_ptr<MethodSymbol> symbol);

  ValueSymbol *getValue(str name);

  friend class Resolver;
  friend class Linker;
  int scopeId = 1;
};
