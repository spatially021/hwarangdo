#pragma once

#include "hrd/IR/MIR/MIRNode.h"
#include "hrd/IR/MIR/MIRProgram.h"
#include <cstddef>

class MIRDebugger {

private:
  MIRProgram *program;
  size_t depth = 0;

  string ident();

private:
  void debugFunction(MIRFunction *func);
  void debugBlock(BasicBlock *block);
  void debugStmt(MIRStmt *stmt);
  void debugTerminator(const MIRTerminator &term);
  void debugValue(MIRValue *value);
  void debugPlace(MIRPlace *place);
  void debugCase(const MIRCase &stmt);

public:
  MIRDebugger(MIRProgram *program);
  void debug();
};