#pragma once

#include "IR/HIR/HIRDecl.h"
#include "IR/HIR/HIRModule.h"
#include "IR/HIR/HIRStmt.h"
#include "IR/HIR/HIRType.h"

class HIRPrinter {
public:
  void print(const HIRModule *module);

private:
  int depth = 0;

  void indent() const;
  void printType(const HIRType *type);
  void printDecl(const HIRDecl *decl);
  void printStmt(const HIRStmt *stmt);
  void printExpr(const HIRExpr *expr);
  void printPattern(const HIRPattern *pattern);
};