#pragma once

#include "hrd/IR/HIR/HIRDecl.h"
#include "hrd/IR/HIR/HIRProgram.h"
#include "hrd/IR/HIR/HIRStmt.h"

class HIRPrinter {
public:
  void print(const HIRProgram *module);

private:
  int depth = 0;

  void indent() const;
  void printDecl(const HIRDecl *decl);
  void printStmt(const HIRStmt *stmt);
  void printExpr(const HIRExpr *expr);
  void printPattern(const HIRPattern *pattern);
};