#include "hrd/IR/llvmIR/llvmCodegen.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include <llvm/IR/Value.h>
#include <stdexcept>

llvm::Function *llvmCodegen::emitDefaultDestroy(TypeSymbol *ty) {
  auto fn = defaultDestroys.at(ty);
  auto *layoutTy = getLayoutType(ty);
  auto *bb = llvm::BasicBlock::Create(context, "entry", fn);
  builder.SetInsertPoint(bb);

  auto *self = fn->getArg(0);

  for (auto it = ty->fields.rbegin(); it != ty->fields.rend(); ++it) {
    auto field = (*it);
    auto *fieldPtr =
        builder.CreateStructGEP(layoutTy, self, field->index, field->name);

    if (needsDestroy(field->typeSymbol)) {
      auto it_ = defaultDestroys.find(field->typeSymbol);
      if (it_ == defaultDestroys.end()) {
        throw runtime_error("fail to find default destroy : " + ty->name);
      }
      auto *fieldDestroy = defaultDestroys.at(field->typeSymbol);
      builder.CreateCall(fieldDestroy, {fieldPtr});
    }
  }

  builder.CreateRetVoid();
  return fn;
}

void llvmCodegen::emitFuncBody(MIRFunction *func) {
  auto fn = funcs.at(func->symbol);
  auto entry = llvm::BasicBlock::Create(context, "entry", fn);
  builder.SetInsertPoint(entry);

  unordered_map<BlockID, llvm::BasicBlock *> blocks;

  for (auto &b : func->blocks) {
    blocks.emplace(b->id, llvm::BasicBlock::Create(
                              context, "bb" + std::to_string(b->id), fn));
  }
  localMap locals;
  ParamMap params;
  std::vector<Cleanup> cleanupStack;
  std::unordered_set<llvm::Value *> canceledCleanups;
  auto argIt = fn->arg_begin();

  llvm::Argument *self = &*argIt++;
  self->setName("self");

  params.emplace(func->symbol->selfReceiver, self);

  FuncContext ctx = {fn,   blocks,       locals,          params,
                     self, cleanupStack, canceledCleanups};

  for (auto p : func->symbol->params) {
    llvm::Argument *arg = &*argIt++;
    arg->setName(p->name);

    auto *slot = createEntryAlloca(fn, getType(p->typeSymbol), p->name);
    builder.CreateStore(arg, slot);

    ctx.params.emplace(p, slot);
  }
  builder.CreateBr(blocks.at(func->entry)); // 또는 func->blocks.front()->id

  for (auto &b : func->blocks) {
    lowerBlock(b.get(), ctx);
  }

  if (entry->getTerminator() == nullptr) {
    builder.SetInsertPoint(entry);
    builder.CreateBr(ctx.blocks.at(func->blocks.front()->id));
  }
}

void llvmCodegen::emitFieldZeroInit(TypeSymbol *owner, llvm::Value *self) {
  auto *layoutTy = getLayoutType(owner);

  for (auto *field : owner->fields) {
    auto *fieldPtr = builder.CreateStructGEP(layoutTy, self, field->index,
                                             field->name + ".zero");

    auto *fieldTy = field->typeSymbol;

    if (needsDestroy(fieldTy)) {
      builder.CreateStore(llvm::Constant::getNullValue(getType(fieldTy)),
                          fieldPtr);
    }

    if (fieldTy->kind == TypeSymbol::TypeKind::STRUCT) {
      auto *init = defaultInits.at(fieldTy);
      builder.CreateCall(init, {fieldPtr});
      continue;
    }
  }
}

void llvmCodegen::addClean(llvm::Value *addr, TypeSymbol *type,
                           FuncContext &ctx) {
  ctx.cleanupStack.push_back({addr, type});
}