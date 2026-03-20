#pragma once

#include "SemanticAnalyzer/Scope.h"
#include "SemanticAnalyzer/symbol/StorageSymbol.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "Symbol.h"
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

protected:
  void _anchor() override {};
};

class EnumVariantSymbol : public ValueSymbol {
public:
  int ordinal;
  TypeSymbol *payloadType = nullptr;
  optional<ValueSymbol> payloadValue = nullopt;
  EnumVariantSymbol() { type = Symbol::SymbolType::ENUM_VARIANT; }

protected:
  void _anchor() override {};
};
