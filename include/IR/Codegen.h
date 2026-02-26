#pragma once
#include "IR/MIR.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <unordered_map>
#include <vector>
using std::vector;

class Codegen {
public:
  Codegen();

  void emitModule(const MirModule &mir);
  void dumpIR();

private:
  llvm::LLVMContext context;
  std::unique_ptr<llvm::Module> module;
  llvm::IRBuilder<> builder;

  std::unordered_map<MirValue *, llvm::Value *> valueMap;

  llvm::Type *toLLVMType(const MirType &type);
  llvm::Function *createLLVMFunction(const MirFunction &fn);
  void emitFunction(const MirFunction &fn);
  void emitBasicBlock(const BasicBlock &bb);
  void emitTerminator(const Terminator &term);

  llvm::Value *toLLVMValue(MirValue *v);

  llvm::Value *OperateI32Binary(BinaryOpValue *bin);
};
