#pragma once

#include "SymbolTable.h"
#include "hrd/AST/Decl.h"
#include "hrd/AST/Stmt.h"
#include "hrd/AST/Visitor.h"
#include "hrd/Recover/BuilderRecover.h"
#include "hrd/SourceSpan.h"
#include "hrd/compiler/CompilerContexts.h"
#include "hrd/util/diagnostic/DiagnosticEngine.h"
#include <cassert>
#include <memory>

class Builder : public ASTVisitor {
public:
  SymbolTable &table;
  TypeSymbol *currentType = nullptr;
  unique_ptr<Scope> rootScope = make_unique<Scope>();
  DiagnosticEngine &engine;
  Builder(BuilderContext &context);

#define AST_NODE(T) void visit(T *node) override;
#include "../AST/ASTNodeList.def"
#undef AST_NODE

  inline void linkRoot() {
    if (!table.main) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S122);
      dia.labels = {
          {SourceSpan(), "main declaration was not found", true},
      };
      dia.helps = {
          "declare exactly one Main class",
      };
      engine.emit(dia);
      recover.recover();
    }

    table.main->rootScope = std::move(rootScope);
  }

private:
  unique_ptr<TypeSymbol> topLevel;
  void extracted();
  void buildMain(ClassDecl *decl);
  BuilderRecover recover;
};
