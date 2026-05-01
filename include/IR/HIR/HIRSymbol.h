#pragma once

#include "IR/HIR/HIRType.h"
#include "string"
#include <memory>
#include <unordered_set>

class MethodSymbol;
class ValueSymbol;
class EnumVariantSymbol;
struct HIRTypeDecl;
struct HIRExpr;
struct HIRLocal;
struct HIRField;

struct InitState {
  bool initialized = false;

  // 배열일 때만 사용
  bool fullyInitialized = false;
  std::unordered_set<uint64_t> initializedIndices;
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
