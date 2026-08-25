#pragma once

#include "hrd/AST/Decl.h"
#include "hrd/AST/Program.h"
#include "hrd/IR/HIR/HIRDecl.h"
#include "hrd/IR/HIR/HIRProgram.h"
#include "hrd/SemanticAnalyzer/SymbolTable/SymbolTable.h"
#include "hrd/SourceSpan.h"
#include "hrd/compiler/CompilerContexts.h"
#include <memory>
class HIRLinker {
public:
  HIRProgram *program = nullptr;
  SourceFile *source = nullptr;
  unique_ptr<HIRSource> hirSource;
  HIRMethodDecl *currentMethod = nullptr;
  SymbolTable &table;
  HIRLinker(HIRContext &ctx, SourceFile *s)
      : program(ctx.program), source(s), table(ctx.table) {
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
