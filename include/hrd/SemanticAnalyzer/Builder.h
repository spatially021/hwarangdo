#pragma once

#include "hrd/AST/Decl.h"
#include "hrd/AST/Stmt.h"
#include "hrd/AST/Visitor.h"
#include "hrd/Recover/BuilderRecover.h"
#include "hrd/SemanticAnalyzer/SymbolTable/SymbolTable.h"
#include "hrd/SourceSpan.h"
#include "hrd/compiler/CompilerContexts.h"
#include "hrd/diagnostic/DiagnosticEngine.h"
#include <cassert>
#include <memory>

class Builder : public ASTVisitor {
public:
  SymbolTable &table;
  TypeSymbol *&currentType;
  unique_ptr<Scope> rootScope = make_unique<Scope>();
  DiagnosticEngine &engine;
  Builder(BuilderContext &context);

#define AST_NODE(T) void visit(T *node) override;
#include "../AST/ASTNodeList.def"
#undef AST_NODE

  inline void linkRoot(bool isCompile) {
    if (table.main == nullptr && !isCompile) {
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
  }

private:
  unique_ptr<TypeSymbol> topLevel;
  void extracted();
  void buildMain(ClassDecl *decl);
  BuilderRecover recover;
};
