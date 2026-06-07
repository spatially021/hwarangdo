#pragma once

#include "SemanticAnalyzer/symbol/TypeSymbol.h"

struct MIRType;

struct MIRLocalDecl {
  ValueSymbol *symbol;
  MIRType *type;
};

struct MIRParam {
  ValueSymbol *symbol;
  MIRType *type;
};

struct MIRField {
  ValueSymbol *symbol;
  MIRType *type;
  uint32_t index;
};

struct MIRType {
  TypeSymbol *type = nullptr;
  vector<MIRField> fields;
};