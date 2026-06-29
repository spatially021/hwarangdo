#pragma once

#include "hrd/IR/HIR/HIRDecl.h"
#include "hrd/IR/HIR/HIRExpr.h"
#include "hrd/IR/HIR/HIRPattern.h"
#include "hrd/IR/HIR/HIRProgram.h"
#include "hrd/IR/HIR/HIRStmt.h"
#include "hrd/IR/HIR/HIRSymbol.h"
#include <cstddef>

class HIRDebugger {
private:
  HIRProgram *program;
  size_t depth = 0;

  string ident();

  void debugType(HIRTypeDecl *type);
  void debugField(HIRField *field);
  void debugMethod(HIRMethodDecl *method);
  void debugLocal(HIRLocal *local);

  void debugBlock(HIRBlockStmt *stmt);
  void debugStmt(HIRStmt *stmt);
  void debugExprStmt(HIRExprStmt *stmt);
  void debugLocalDeclStmt(HIRLocalDeclStmt *stmt);
  void debugIfStmt(HIRIfStmt *stmt);
  void debugWhileStmt(HIRWhileStmt *stmt);
  void debugForRangeStmt(HIRForRangeStmt *stmt);
  void debugReturnStmt(HIRReturnStmt *stmt);
  void debugSwitchStmt(HIRSwitchStmt *stmt);
  void debugCase(HIRCase *stmt);
  void debugValueTransferStmt(HIRValueTransferStmt *stmt);
  void debugDestroyStmt(HIRDestroyStmt *stmt);
  void debugAssignStmt(HIRAssignStmt *stmt);
  void debugCompoundAssignStmt(HIRCompoundAssignStmt *stmt);

  void debugExpr(HIRExpr *expr);
  void debugCasePattern(HIRCasePattern *expr);

public:
  HIRDebugger(HIRProgram *program);
  void debug();
};
