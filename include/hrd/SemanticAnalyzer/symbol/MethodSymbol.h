#pragma once

#include "Symbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/enums/MethodKind.h"
#include <vector>
class Scope;

class MethodSymbol : public Symbol {
public:
  MethodSymbol() { type = Symbol::SymbolType::METHOD; }

  TypeSymbol *owner = nullptr;
  TypeSymbol *declType = nullptr;
  TypeSymbol *returnType = nullptr;
  ASTNode *decl = nullptr;
  vector<ValueSymbol *> params;
  Expr::State state = Expr::State::RESOLVED;
  Scope *scope = nullptr;
  Scope *selfScope = nullptr;
  MethodKind methodKind;

  bool isExtern = false;
  bool isFrame = false;
  bool isOverride = false;
  std::vector<ReturnStmt *> returns;
  AModifier modifier = AModifier::PUBLIC;

  bool isRuntime = false;

  ValueSymbol *selfReceiver = nullptr;

protected:
  void _anchor() override {};
};
