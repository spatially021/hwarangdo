#pragma once

#include "Symbol.h"
#include "hrd/AST/Expr.h"
#include "hrd/SemanticAnalyzer/ResolvedLit.h"
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
  bool isConst = false;
  SourceSpan nameSpan;

  uint32_t index = 0;

protected:
  void _anchor() override {};
};

class EnumVariantSymbol : public ValueSymbol {
public:
  uint32_t ordinal;
  TypeSymbol *payloadType = nullptr;
  EnumVariantSymbol() { type = Symbol::SymbolType::ENUM_VARIANT; }

protected:
  void _anchor() override {};
};

class ParamSymbol : public ValueSymbol {
public:
  variant<std::monostate, LiteralExpr *, CallExpr *> defaultValue;

protected:
  void _anchor() override {};
};
