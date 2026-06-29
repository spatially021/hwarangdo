#pragma once

#include "Symbol.h"
#include "hrd/SemanticAnalyzer/symbol/StorageSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SourceSpan.h"
#include "hrd/enums/AccessModifier.h"
#include <cstdint>
#include <variant>

class ValueSymbol : public Symbol {
public:
  ValueSymbol() { type = Symbol::SymbolType::VALUE; }

  enum class Kind {
    VAR,
    TRAITSIG,
    PARAM,
  } kind;

  ASTNode *node = nullptr;
  TypeSymbol *typeSymbol = nullptr;
  std::variant<Scope *, TypeSymbol *, StorageSymbol *> owner;
  bool isRoot = false;
  AModifier modifier = AModifier::PUBLIC;
  bool isPayload = false;
  SourceSpan nameSpan;

  uint32_t index = 0;

protected:
  void _anchor() override {};
};

class EnumVariantSymbol : public ValueSymbol {
public:
  int ordinal;
  TypeSymbol *payloadType = nullptr;
  EnumVariantSymbol() { type = Symbol::SymbolType::ENUM_VARIANT; }

protected:
  void _anchor() override {};
};
