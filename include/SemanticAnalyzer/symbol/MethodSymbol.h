#pragma once

#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "SemanticAnalyzer/symbol/ValueSymbol.h"
#include "Symbol.h"
#include <vector>
class Scope;

class MethodSymbol : public Symbol {
public:
  MethodSymbol() { type = Symbol::SymbolType::METHOD; }

  TypeSymbol *onwer = nullptr;
  TypeSymbol *declType = nullptr;
  TypeSymbol *returnType = nullptr;
  ASTNode *decl = nullptr;
  vector<ValueSymbol *> params;
  Expr::State state = Expr::State::RESOLVED;
  Scope *scope = nullptr;
  Scope *selfScope = nullptr;

  bool isInit = false;

  bool isExtern = false;
  bool isFrame = false;
  bool isOverride = false;
  std::vector<ReturnStmt *> returns;
  AModifier modifier = AModifier::PUBLIC;

  ValueSymbol *selfReceiver = nullptr;

protected:
  void _anchor() override {};
};
