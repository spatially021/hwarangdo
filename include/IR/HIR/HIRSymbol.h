#pragma once

#include "IR/HIR/HIRType.h"
#include "string"

struct FieldSymbol;
struct MethodSymbol;
struct ValueSymbol;
struct EnumVariantSymbol;

struct HIRField {
  int id = -1;
  std::string name;
  HIRType *type = nullptr;
  FieldSymbol *symbol = nullptr;
  bool isMutable = true;
};

struct HIRParam {
  int id = -1;
  std::string name;
  HIRType *type = nullptr;
  ValueSymbol *symbol = nullptr;
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

struct HIREnumVariant {
  int id = -1;
  std::string name;
  HIREnumType *owner = nullptr;
  EnumVariantSymbol *symbol = nullptr;
};