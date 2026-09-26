#include "hrd/IR/MIR/MIRExpr.h"
#include "hrd/IR/llvmIR/llvmCodegen.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"

LoweredValue llvmCodegen::lowerSpawnExpr(MIRSpawnExpr *expr, FuncContext &ctx) {
  auto *entityType = resolveType(expr->entityType, ctx);

  llvm::Type *llvmEntityTy = nullptr;

  if (auto *generic = dynamic_cast<GenericSymbol *>(entityType)) {
    llvmEntityTy = getOrCreateGeneric(generic);
  } else {
    llvmEntityTy = getLayoutType(entityType);
  }

  auto *entityOrigin = entityType;

  if (auto *generic = dynamic_cast<GenericSymbol *>(entityType)) {
    entityOrigin = generic->origin;
  }

  llvm::Value *size = getSizeOf(llvmEntityTy);

  llvm::FunctionCallee mallocFn = getOrDeclareMalloc();
  llvm::Value *raw = builder.CreateCall(mallocFn, {size});
  llvm::Value *obj = builder.CreateBitCast(raw, builder.getPtrTy());

  builder.CreateMemSet(obj, llvm::ConstantInt::get(builder.getInt8Ty(), 0),
                       size, llvm::MaybeAlign(8));

  std::vector<llvm::Value *> args;
  args.push_back(obj);
  vector<MIRValue *> values;
  for (auto &arg : expr->args) {
    values.push_back(arg.get());
  }

  auto out = lowerArgs(args, values, ctx);

  auto initFieldIt = defaultInits.find(entityType);
  if (initFieldIt != defaultInits.end()) {
    builder.CreateCall(initFieldIt->second, {obj});
  }

  if (expr->initMethod != nullptr) {
    llvm::Function *initFn = nullptr;
    if (auto *generic = dynamic_cast<GenericSymbol *>(entityType)) {
      auto key = GenericMethodKey(generic, expr->initMethod);
      auto methodIt = genericMethodMap.find(key);
      if (methodIt == genericMethodMap.end()) {
        Error::internal("generic struct init method not found: " +
                        expr->initMethod->name);
      }
      initFn = methodIt->second;
    } else {
      initFn = getOrDeclareFunction(expr->initMethod);
    }

    builder.CreateCall(initFn, args);

    for (auto &o : out) {
      builder.CreateCall(defaultDestroys.at(o.type), {o.addr});
    }
  }

  auto destroyIt = defaultDestroys.find(entityType);
  if (destroyIt == defaultDestroys.end()) {
    throw std::runtime_error("fail to find entity destroy");
  }
  auto *on = dynamic_cast<ObjectType *>(entityOrigin)->onDestroy.get();

  llvm::Function *onDestroyFn = nullptr;

  if (on != nullptr) {
    if (auto *generic = dynamic_cast<GenericSymbol *>(entityType)) {
      auto key = GenericMethodKey(generic, on);
      auto onDestroyIt = genericMethodMap.find(key);
      if (onDestroyIt == genericMethodMap.end()) {
        throw std::runtime_error("fail to find generic entity onDestroy");
      }
      onDestroyFn = onDestroyIt->second;
    } else {
      auto onDestroyIt = funcs.find(on);
      if (onDestroyIt == funcs.end()) {
        throw std::runtime_error("fail to find entity onDestroy");
      }
      onDestroyFn = onDestroyIt->second;
    }
  }

  llvm::Value *onDestroyArg =
      onDestroyFn != nullptr
          ? static_cast<llvm::Value *>(onDestroyFn)
          : llvm::ConstantPointerNull::get(builder.getPtrTy());

  llvm::FunctionCallee spawnRaw = getOrDeclareWorldSpawnRaw();

  return {builder.CreateCall(spawnRaw, {obj, destroyIt->second, onDestroyArg})};
}

llvm::FunctionCallee llvmCodegen::getOrDeclareWorldSpawnRaw() {
  auto *fnTy =
      llvm::FunctionType::get(builder.getInt64Ty(), // HrdHandle
                              {
                                  builder.getPtrTy(), // entity ptr
                                  builder.getPtrTy(), // destroy fn ptr
                                  builder.getPtrTy(), // onDestroy fn ptr
                              },
                              false);

  return llvmModule->getOrInsertFunction("hrd_world_spawn_raw", fnTy);
}

LoweredValue llvmCodegen::lowerViewExpr(MIRViewExpr *expr, FuncContext &ctx) {
  auto handle = lowerValue(expr->handle.get(), ctx);

  llvm::FunctionCallee viewRaw = getOrDeclareWorldViewRaw();

  llvm::Value *rawPtr = builder.CreateCall(viewRaw,
                                           {
                                               handle.value,
                                           },
                                           "view.raw");

  return {rawPtr};
}

llvm::FunctionCallee llvmCodegen::getOrDeclareWorldViewRaw() {
  auto *ptrTy = llvm::PointerType::getUnqual(context);

  return llvmModule->getOrInsertFunction(
      "hrd_world_view_raw",
      llvm::FunctionType::get(ptrTy,
                              {
                                  getHrdHandleType(), // HrdHandle
                              },
                              false));
}
void llvmCodegen::lowerDestroy(MIRDestroyStmt *stmt, FuncContext &ctx) {
  auto handle = lowerValue(stmt->handlePlace.get(), ctx);
  auto destroy = getOrDeclareWorldDestroyRaw();
  builder.CreateCall(destroy, {handle.value});
}

llvm::FunctionCallee llvmCodegen::getOrDeclareWorldDestroyRaw() {
  return llvmModule->getOrInsertFunction(
      "hrd_world_destroy_entity_raw",
      llvm::FunctionType::get(builder.getVoidTy(),
                              {
                                  getHrdHandleType(),
                              },
                              false));
}