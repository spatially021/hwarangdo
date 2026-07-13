#include "hrd/IR/llvmIR/llvmCodegen.h"

MethodSymbol *llvmCodegen::getMainMethod(const string &name) {

  if (name == "update") {
    return table->main->update;
  }

  if (name == "init") {
    return table->main->init;
  }

  Error::internal("Main method not found: " + name);
}

void llvmCodegen::generateEntryMain(MainSymbol *mainType) {
  auto i32Ty = llvm::Type::getInt32Ty(context);
  auto i1Ty = llvm::Type::getInt1Ty(context);
  auto ptrTy = llvm::PointerType::getUnqual(context);

  auto mainFnTy = llvm::FunctionType::get(i32Ty, {}, false);
  auto mainFn = llvm::Function::Create(
      mainFnTy, llvm::Function::ExternalLinkage, "main", llvmModule.get());

  auto entryBB = llvm::BasicBlock::Create(context, "entry", mainFn);
  auto loopCondBB = llvm::BasicBlock::Create(context, "loop.cond", mainFn);
  auto loopBodyBB = llvm::BasicBlock::Create(context, "loop.body", mainFn);
  auto exitBB = llvm::BasicBlock::Create(context, "exit", mainFn);

  builder.SetInsertPoint(entryBB);

  auto worldCreateFn = getRuntimeFunc(
      "hrd_world_create", llvm::FunctionType::get(ptrTy, {}, false));

  auto worldRunningFn = getRuntimeFunc(
      "hrd_world_running", llvm::FunctionType::get(i1Ty, {}, false));

  auto worldDestroyFn =
      getRuntimeFunc("hrd_world_destroy",
                     llvm::FunctionType::get(builder.getVoidTy(), {}, false));

  builder.CreateCall(worldCreateFn);

  auto worldFlustDestroy =
      getRuntimeFunc("hrd_world_flush_destroy",
                     llvm::FunctionType::get(builder.getVoidTy(), {}, false));

  // 임시: Main 객체 생성 방식은 나중에 world.spawn으로 교체 가능
  llvm::Type *mainLlvmTy = getLayoutType(mainType);
  llvm::Value *mainObj = builder.CreateAlloca(mainLlvmTy);

  auto init = getMainMethod("init");
  auto mainUpdate = funcs.at(getMainMethod("update"));

  auto defaultInit = defaultInits.at(mainType);
  builder.CreateCall(defaultInit, {mainObj});
  if (init) {
    auto mainInit = funcs.at(init);
    builder.CreateCall(mainInit, {mainObj});
  }

  builder.CreateBr(loopCondBB);

  builder.SetInsertPoint(loopCondBB);
  llvm::Value *running = builder.CreateCall(worldRunningFn, {});
  builder.CreateCondBr(running, loopBodyBB, exitBB);
  builder.SetInsertPoint(loopBodyBB);
  { // 런타임 메인 루프
    builder.CreateCall(mainUpdate, {mainObj});
    builder.CreateCall(worldFlustDestroy, {});
  }

  builder.CreateBr(loopCondBB);

  builder.SetInsertPoint(exitBB);
  builder.CreateCall(defaultDestroys.at(mainType), {mainObj});
  builder.CreateCall(worldDestroyFn, {});
  builder.CreateRet(builder.getInt32(0));
}
