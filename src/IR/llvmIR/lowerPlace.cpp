#include "hrd/IR/MIR/MIRExpr.h"
#include "hrd/IR/llvmIR/llvmCodegen.h"
#include <llvm/IR/Value.h>
llvm::Value *llvmCodegen::lowerPlace(MIRPlace *place, FuncContext &ctx) {

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

llvm::Value *llvmCodegen::lowerLocalPlace(MIRLocalPlace *place,
                                          FuncContext &ctx) {
  auto it = ctx.locals.find(place->symbol);
  if (it == ctx.locals.end()) {
    Error::internal("fail to find local place in LLVM lowering: " +
                    place->symbol->name);
  }
  return it->second;
}

llvm::Value *llvmCodegen::lowerParamPlace(MIRParamPlace *place,
                                          FuncContext &ctx) {
  auto it = ctx.params.find(place->symbol);
  if (it == ctx.params.end()) {
    return ctx.self;
  }
  return it->second;
}

llvm::Value *llvmCodegen::lowerFieldPlace(MIRFieldPlace *place,
                                          FuncContext &ctx) {
  auto *base = lowerPlace(place->base.get(), ctx);

  auto *baseType = place->ownType;
  auto *baseLayoutTy = getLayoutType(baseType);

  if (baseType->kind == TypeSymbol::TypeKind::CLASS &&
      dynamic_cast<MIRLocalPlace *>(place->base.get())) {
    base = builder.CreateLoad(builder.getPtrTy(), base, "entity.local.ptr");
  }

  return builder.CreateStructGEP(baseLayoutTy, base, place->symbol->index,
                                 place->symbol->name);
}

llvm::Value *llvmCodegen::lowerArrayAccessPlace(MIRArrayAccessPlace *place,
                                                FuncContext &ctx) {
  auto *basePtr = lowerPlace(place->base.get(), ctx);
  auto index = lowerValue(place->index.get(), ctx);

  return builder.CreateInBoundsGEP(getLayoutType(place->ownType), basePtr,
                                   {builder.getInt32(0), index.value});
}

llvm::Value *llvmCodegen::lowerRootPlace(MIRRootPlace *place, FuncContext &) {
  auto it = roots.find(place->symbol);
  if (it == roots.end()) {
    Error::internal("fail to find root place in LLVM lowering: " +
                    place->symbol->name);
  }
  return it->second;
}