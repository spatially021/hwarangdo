#pragma once

#include "AST/Program.h"
#include "IR/HIR/HIRProgram.h"
#include <memory>
class HIRLinker {
public:
  HIRProgram *program = nullptr;
  SourceFile *source = nullptr;
  unique_ptr<HIRSource> hirSource;

  HIRLinker(HIRProgram *p, SourceFile *s) : program(p), source(s) {
    hirSource = make_unique<HIRSource>(s);
  }

  TypeSymbol *getTypeSymbolFromDecl(Decl *decl, HIRTypeDeclKind &kind);
  HIREntityType *lowerEntityType(TypeSymbol *symbol);
  HIRType *getOrCreateType(TypeSymbol *symbol);
  HIRType *lowerType(TypeSymbol *symbol);

  void lowerTypeShell(Decl *decl);

  unique_ptr<HIRSource> link();
};