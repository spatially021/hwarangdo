#include "hrd/IR/MIR/MIRExpr.h"
#include "hrd/IR/llvmIR/llvmCodegen.h"
#include "hrd/util/Error.h"
#include <cassert>
#include <cstdint>
#include <llvm/IR/Value.h>
LoweredPlace llvmCodegen::lowerPlace(MIRPlace *place, FuncContext &ctx) {

  if (auto *p = dynamic_cast<MIRArrayAccessPlace *>(place)) {
    return lowerArrayAccessPlace(p, ctx);
  }

  if (auto *p = dynamic_cast<MIRLocalPlace *>(place)) {
    return lowerLocalPlace(p, ctx);
  }

  if (auto *p = dynamic_cast<MIRParamPlace *>(place)) {
    return lowerParamPlace(p, ctx);
  }

  if (auto *p = dynamic_cast<MIRFieldPlace *>(place)) {
    return lowerFieldPlace(p, ctx);
  }

  if (auto *p = dynamic_cast<MIRRootPlace *>(place)) {
    return lowerRootPlace(p, ctx);
  }

  Error::internal("unknown MIRPlace in LLVM lowering");
}

LoweredPlace llvmCodegen::lowerLocalPlace(MIRLocalPlace *place,
                                          FuncContext &ctx) {
  auto it = ctx.locals.find(place->symbol);
  if (it == ctx.locals.end()) {
    Error::internal("fail to find local place in LLVM lowering: " +
                    place->symbol->name);
  }
  return {it->second, place->symbol->typeSymbol};
}

LoweredPlace llvmCodegen::lowerParamPlace(MIRParamPlace *place,
                                          FuncContext &ctx) {
  if (place->symbol == nullptr) {
    return {ctx.self, ctx.selfType};
  }
  auto it = ctx.params.find(place->symbol);
  if (it == ctx.params.end()) {
    Error::internal("param's symbol is nullptr");
  }
  return {it->second, place->symbol->typeSymbol};
}

LoweredPlace llvmCodegen::lowerFieldPlace(MIRFieldPlace *place,
                                          FuncContext &ctx) {
  auto lowered = lowerPlace(place->base.get(), ctx);
  auto base = lowered.dst;
  auto *baseType = place->ownType;
  auto *baseLayoutTy = getLayoutType(baseType);

  if (baseType->kind == TypeKind::CLASS &&
      dynamic_cast<MIRLocalPlace *>(place->base.get())) {
    base = builder.CreateLoad(builder.getPtrTy(), base, "entity.local.ptr");
  }

  return {builder.CreateStructGEP(baseLayoutTy, base, place->symbol->index,
                                  place->symbol->name),
          place->symbol->typeSymbol};
}

LoweredPlace llvmCodegen::lowerArrayAccessPlace(MIRArrayAccessPlace *place,
                                                FuncContext &ctx) {
  auto lowered = lowerPlace(place->base.get(), ctx);
  auto *basePtr = lowered.dst;

  auto index = lowerValue(place->index.get(), ctx);
  auto *indexValue = index.value;

  uint64_t length = 0;

  if (auto arr = dynamic_cast<ArrayTypeSymbol *>(place->symbol->typeSymbol)) {
    length = arr->sizeValue.getZExtValue();
  } else {
    Error::internal("array's base is not arrayTypeSymbol");
  }

  auto *func = builder.GetInsertBlock()->getParent();

  auto *failBlock =
      llvm::BasicBlock::Create(context, "array.bounds.fail", func);

  auto *okBlock = llvm::BasicBlock::Create(context, "array.bounds.ok", func);

  auto *zero = llvm::ConstantInt::get(indexValue->getType(), 0);

  auto *lenValue = llvm::ConstantInt::get(indexValue->getType(), length);

  auto *negative = builder.CreateICmpSLT(indexValue, zero);

  auto *tooLarge = builder.CreateICmpSGE(indexValue, lenValue);

  auto *invalid = builder.CreateOr(negative, tooLarge);

  builder.CreateCondBr(invalid, failBlock, okBlock);

  // fail
  builder.SetInsertPoint(failBlock);

  auto *boundsErrorType = llvm::FunctionType::get(builder.getVoidTy(),
                                                  {
                                                      builder.getInt64Ty(),
                                                      builder.getInt64Ty(),
                                                  },
                                                  false);

  auto *boundsError = getRuntimeFunc("hrd_array_bounds_error", boundsErrorType);

  auto *index64 = builder.CreateSExtOrTrunc(indexValue, builder.getInt64Ty());

  builder.CreateCall(boundsError, {
                                      index64,
                                      builder.getInt64(length),
                                  });

  builder.CreateUnreachable();

  // success
  builder.SetInsertPoint(okBlock);

  auto *elementPtr =
      builder.CreateInBoundsGEP(getLayoutType(place->ownType), basePtr,
                                {
                                    builder.getInt32(0),
                                    indexValue,
                                });

  return {
      elementPtr,
      place->symbol->typeSymbol,
  };
}

LoweredPlace llvmCodegen::lowerRootPlace(MIRRootPlace *place, FuncContext &) {
  auto it = roots.find(place->symbol);
  if (it == roots.end()) {
    Error::internal("fail to find root place in LLVM lowering: " +
                    place->symbol->name);
  }
  return {it->second, place->symbol->typeSymbol};
}