#pragma once

#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "Symbol.h"
class Scope;

class MethodSymbol : public Symbol {
public:
  MethodSymbol() { type = Symbol::SymbolType::METHOD; }

  TypeSymbol *onwer = nullptr;
  TypeSymbol *declType = nullptr;
  TypeSymbol *returnType = nullptr;
  ASTNode *decl = nullptr;
  vector<TypeSymbol *> paramTypes;
  Expr::State state = Expr::State::RESOLVED;
  Scope *scope = nullptr;
  Scope *selfScope = nullptr;

  bool isInit = false;

  bool isExtern = false;
  bool isFrame = false;
  bool isOverride = false;
  std::vector<ReturnStmt *> returns;
  AModifier modifier = AModifier::PUBLIC;

protected:
  void _anchor() override {};
};
