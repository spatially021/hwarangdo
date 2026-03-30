#pragma once

#include "AST/Decl.h"
#include "AST/Stmt.h"
#include "AST/Visitor.h"
#include "SymbolTable.h"
#include "util/Error.h"
#include <cassert>
#include <memory>

class Builder : public ASTVisitor {
public:
  SymbolTable *table = nullptr;
  TypeSymbol *currentType = nullptr;
  unique_ptr<Scope> rootScope = make_unique<Scope>();
  Builder(SymbolTable *table);

#define AST_NODE(T) void visit(T *node) override;
#include "../AST/ASTNodeList.def"
#undef AST_NODE

  inline void linkRoot() {
    if (!table->main) {
      Error::diagnostic({}, "has no main");
    }
    table->main->rootScope = std::move(rootScope);
  }

private:
  unique_ptr<TypeSymbol> topLevel;
  void extracted();
  void buildMain(ClassDecl *decl);
  bool canInnerDecl(Decl *decl);
};
