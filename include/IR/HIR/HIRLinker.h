#pragma once

#include "AST/Decl.h"
#include "AST/Program.h"
#include "IR/HIR/HIRDecl.h"
#include "IR/HIR/HIRProgram.h"
#include "SemanticAnalyzer/SymbolTable.h"
#include <memory>
class HIRLinker {
public:
  HIRProgram *program = nullptr;
  SourceFile *source = nullptr;
  unique_ptr<HIRSource> hirSource;
  HIRMethodDecl *currentMethod = nullptr;
  SymbolTable *table;
  HIRLinker(HIRProgram *p, SourceFile *s, SymbolTable *t)
      : program(p), source(s), table(t) {
    hirSource = make_unique<HIRSource>(s);
  }

  TypeSymbol *getTypeSymbolFromDecl(Decl *decl, HIRTypeDeclKind &kind);
  HIREntityType *lowerEntityType(TypeSymbol *symbol);
  HIRType *getOrCreateType(TypeSymbol *symbol);
  HIRType *lowerType(TypeSymbol *symbol);
  void lowerMethodDeclShell(FuncDecl *decl, HIRTypeDecl *currentType);

  unique_ptr<HIRParam> lowerParam(Param *decl);
  void lowerTypeShell(Decl *decl);

  unique_ptr<HIRSource> link();
};