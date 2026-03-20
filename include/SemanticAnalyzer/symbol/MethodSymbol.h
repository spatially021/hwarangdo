#pragma once

#include "Symbol.h"
class Scope;

class MethodSymbol : public Symbol {
public:
  MethodSymbol() { type = Symbol::SymbolType::METHOD; }

  TypeSymbol *onwer = nullptr;
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

protected:
  void _anchor() override {};
};
