#pragma once

#include "AST/Decl.h"
#include "AST/Program.h"
#include "IR/HIR/HIRDecl.h"
#include "IR/HIR/HIRProgram.h"
#include "SemanticAnalyzer/SymbolTable.h"
#include "SourceSpan.h"
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
    SourceSpan span;
    span.path = s->path;
    span.lineStart = 0;
    hirSource = make_unique<HIRSource>(span, s);
  }

  TypeSymbol *getTypeSymbolFromDecl(Decl *decl, HIRTypeDeclKind &kind);
  void lowerMethodDeclShell(FuncDecl *decl, HIRTypeDecl *currentType);

  unique_ptr<HIRParam> lowerParam(Param *decl);
  void lowerTypeShell(Decl *decl);
  void lowerField(VarDecl *decl, HIRTypeDecl *type);
  unique_ptr<HIRSource> link();

  void linkSecondPass();
};
