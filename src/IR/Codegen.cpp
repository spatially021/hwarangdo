#include "IR/Codegen.h"
#include "IR/MIR.h"
#include <cassert>
#include <llvm/IR/Value.h>
#include <llvm/Support/ErrorHandling.h>

Codegen::Codegen()
    : module(std::make_unique<llvm::Module>("hgm_module", context)),
      builder(context) {}

llvm::Type *Codegen::toLLVMType(const MirType &type) {
  switch (type.kind) {
  case MirTypeKind::I32:
    return llvm::Type::getInt32Ty(context);
  }
  llvm_unreachable("Unknown MIR type");
}

llvm::Function *Codegen::createLLVMFunction(const MirFunction &fn) {
  llvm::FunctionType *fnType =
      llvm::FunctionType::get(toLLVMType(fn.returnType), {}, false);

  return llvm::Function::Create(fnType, llvm::Function::ExternalLinkage,
                                fn.name, module.get());
}

void Codegen::emitModule(const MirModule &mir) {
  for (auto &fn : mir.functions) {
    emitFunction(*fn);
  }
}

void Codegen::emitFunction(const MirFunction &fn) {
  valueMap.clear();

  llvm::Function *llvmFn = createLLVMFunction(fn);

  for (auto &bb : fn.blocks) {
    auto *llvmBB = llvm::BasicBlock::Create(context, "entry", llvmFn);

    builder.SetInsertPoint(llvmBB);
    emitBasicBlock(*bb);
  }

  llvm::verifyFunction(*llvmFn);
}

void Codegen::emitBasicBlock(const BasicBlock &bb) {
  for (auto &val : bb.values) {
    if (auto *c = dynamic_cast<ConstantIntValue *>(val.get())) {
      llvm::Value *llvmVal =
          llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), c->value);

      valueMap[c] = llvmVal;
    } // BinaryOp
    else if (auto *bin = dynamic_cast<BinaryOpValue *>(val.get())) {
      llvm::Value *result = nullptr;

      switch (bin->lhs->type.kind) {

      case MirTypeKind::I32:
        result = OperateI32Binary(bin);
        break;
      }
      valueMap[bin] = result;
    }
  }

  emitTerminator(*bb.terminator);
}

void Codegen::emitTerminator(const Terminator &term) {
  if (term.kind == TermKind::Return) {
    auto &ret = static_cast<const ReturnTerm &>(term);
    builder.CreateRet(toLLVMValue(ret.value));
  }
}

llvm::Value *Codegen::toLLVMValue(MirValue *v) {
  auto it = valueMap.find(v);
  assert(it != valueMap.end());
  return it->second;
}

void Codegen::dumpIR() { module->print(llvm::outs(), nullptr); }

llvm::Value *Codegen::OperateI32Binary(BinaryOpValue *bin) {

  llvm::Value *lhs = toLLVMValue(bin->lhs);
  llvm::Value *rhs = toLLVMValue(bin->rhs);

  switch (bin->op) {
  case BinaryOpKind::Add:
    return builder.CreateAdd(lhs, rhs);
  case BinaryOpKind::Sub:
    return builder.CreateSub(lhs, rhs);
  case BinaryOpKind::Mul:
    return builder.CreateMul(lhs, rhs);
  case BinaryOpKind::Div:
    return builder.CreateSDiv(lhs, rhs);
  case BinaryOpKind::Rem:
    return builder.CreateSRem(lhs, rhs);
  case BinaryOpKind::Eq:
    return builder.CreateICmpEQ(lhs, rhs);
  case BinaryOpKind::Lt:
    return builder.CreateICmpSLT(lhs, rhs);
  case BinaryOpKind::Le:
    return builder.CreateICmpSLE(lhs, rhs);
  case BinaryOpKind::Gt:
    return builder.CreateICmpSGT(lhs, rhs);
  case BinaryOpKind::Ge:
    return builder.CreateICmpSGE(lhs, rhs);
  case BinaryOpKind::And:
  case BinaryOpKind::Or:
  case BinaryOpKind::Ne:
    assert(false && "Logical op used on I32");
  }
}