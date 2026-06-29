#pragma once

#include "hrd/IR/HIR/HIRType.h"
#include "string"
#include <llvm/ADT/APInt.h>
#include <memory>
#include <unordered_set>

class MethodSymbol;
class ValueSymbol;
class EnumVariantSymbol;
struct HIRTypeDecl;
struct HIRExpr;
struct HIRLocal;
struct HIRField;

struct APIntHash {
  size_t operator()(const llvm::APInt &v) const { return llvm::hash_value(v); }
};

struct APIntEqual {
  bool operator()(const llvm::APInt &a, const llvm::APInt &b) const {
    return a == b;
  }
};

using APIntSet = std::unordered_set<llvm::APInt, APIntHash, APIntEqual>;

struct InitState {
  bool initialized = false;

  // 배열일 때만 사용
  bool fullyInitialized = false;
  APIntSet initializedIndices;
  InitState(bool i, bool f = false) : initialized(i), fullyInitialized(f) {}
};

struct HIRField {
  int id = -1;
  std::string name;
  HIRType *type = nullptr;
  ValueSymbol *symbol = nullptr;
  bool isMutable = true;
  bool isInitialized = false;
};

struct HIRParam {
  int id = -1;
  std::string name;
  HIRType *type = nullptr;
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
  HIRType *type = nullptr;
  ValueSymbol *symbol = nullptr;
  HIRLocalKind kind = HIRLocalKind::Local;
  bool isMutable = true;
  bool isInitialized = false;
  bool isCaseValue = false;
};

// struct HIRRoot {
//   int id = -1;
//   std::string name;
//   HIRType *type = nullptr;
//   ValueSymbol *symbol = nullptr;
//   bool isMutable = true;
//   bool isInitialized = false;
// };

enum class HIREnumVariantKind {
  Unit,    // payload 없음
  Payload, // payload 하나 있음
};

struct HIREnumVariant {
  int id = -1;
  std::string name;
  HIREnumVariantKind kind;
  HIRTypeDecl *owner = nullptr;
  EnumVariantSymbol *symbol = nullptr;
  HIRType *payloadType = nullptr;
};
