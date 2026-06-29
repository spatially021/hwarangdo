#include "hrd/IR/MIR/MIRExpr.h"
#include "hrd/IR/llvmIR/llvmCodegen.h"
#include <llvm/IR/Value.h>

llvm::Value *llvmCodegen::lowerPlace(MIRPlace *place, FuncContext &ctx) {
  if (auto *p = dynamic_cast<MIRLocalPlace *>(place)) {
    return lowerLocalPlace(p, ctx);
  }

  if (auto *p = dynamic_cast<MIRParamPlace *>(place)) {
    return lowerParamPlace(p, ctx);
  }

  if (auto *p = dynamic_cast<MIRArrayAccessPlace *>(place)) {
    return lowerArrayAccessPlace(p, ctx);
  }

  if (auto *p = dynamic_cast<MIRFieldPlace *>(place)) {
    return lowerFieldPlace(p, ctx);
  }

  if (auto *p = dynamic_cast<MIRRootPlace *>(place)) {
    return lowerRootPlace(p, ctx);
  }

  Error::internal("unknown MIRPlace in LLVM lowering");
}

llvm::Value *llvmCodegen::lowerLocalPlace(MIRLocalPlace *place,
                                          FuncContext &ctx) {
  return ctx.locals.at(place->symbol);
}

llvm::Value *llvmCodegen::lowerParamPlace(MIRParamPlace *place,
                                          FuncContext &ctx) {
  return ctx.params.at(place->symbol);
}

llvm::Value *llvmCodegen::lowerFieldPlace(MIRFieldPlace *place,
                                          FuncContext &ctx) {
  auto base = lowerPlace(place->base.get(), ctx);
  auto ownType = getType(place->base->symbol->typeSymbol);

  return builder.CreateStructGEP(ownType, base, place->symbol->index);
}

llvm::Value *llvmCodegen::lowerArrayAccessPlace(MIRArrayAccessPlace *place,
                                                FuncContext &ctx) {
  auto basePtr = lowerPlace(place->base.get(), ctx);
  auto index = lowerValue(place->index.get(), ctx);

  return builder.CreateInBoundsGEP(getType(place->base->symbol->typeSymbol),
                                   basePtr, {builder.getInt32(0), index});
}

llvm::Value *llvmCodegen::lowerRootPlace(MIRRootPlace *place, FuncContext &) {
  return roots.at(place->symbol);
}
