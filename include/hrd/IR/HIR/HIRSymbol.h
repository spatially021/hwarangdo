#pragma once

#include "string"
#include <llvm/ADT/APInt.h>
#include <llvm/ADT/Hashing.h>
#include <memory>
#include <unordered_set>

class MethodSymbol;
class ValueSymbol;
class EnumVariantSymbol;
class TypeSymbol;
struct HIRTypeDecl;
struct HIRExpr;
struct HIRLocal;

struct APIntHash {
  size_t operator()(const llvm::APInt &v) const { return llvm::hash_value(v); }
};

struct APIntEqual {
  bool operator()(const llvm::APInt &a, const llvm::APInt &b) const {
    return a == b;
  }
};

using APIntSet = std::unordered_set<llvm::APInt, APIntHash, APIntEqual>;

struct HIRParam {
  int id = -1;
  std::string name;
  TypeSymbol *type = nullptr;
  ValueSymbol *symbol = nullptr;
  std::unique_ptr<HIRExpr> defaultValue = nullptr;
};

enum class HIRLocalKind {
  Local,
  Temp,
};

struct HIRLocal {
  int id = -1;
  std::string name;
  TypeSymbol *type = nullptr;
  ValueSymbol *symbol = nullptr;
  HIRLocalKind kind = HIRLocalKind::Local;
  bool isInitialized = false;
  bool isCaseValue = false;
};
