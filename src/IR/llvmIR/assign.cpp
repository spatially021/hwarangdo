
#include "hrd/IR/MIR/MIRExpr.h"
#include "hrd/IR/llvmIR/llvmCodegen.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/util/Error.h"
#include <llvm/IR/Value.h>
void llvmCodegen::assign(LoweredPlace dst, LoweredValue rhs, TypeSymbol *type,
                         FuncContext &ctx) {

  if (auto *arr = dynamic_cast<ArrayTypeSymbol *>(dst.type)) {
    if (arr->baseType == type) {
      if (arr->sizeValue.getActiveBits() > sizeof(uint64_t) * 8) {
        Error::internal("too big arr size");
      }

      const uint64_t size = arr->sizeValue.getZExtValue();
      auto *baseType = arr->baseType;

      LoweredValue elementRhs = rhs;
      if (needsDestroy(baseType)) {
        elementRhs.category = MIRValueCategory::Borrowed;
      }

      auto *arrayTy = getType(arr);

      for (uint64_t i = 0; i < size; ++i) {
        auto *place = builder.CreateInBoundsGEP(
            arrayTy, dst.dst, {builder.getInt32(0), builder.getInt64(i)});

        assign({place, baseType}, elementRhs, baseType, ctx);
      }
      return;
    }
  }
  if (dynamic_cast<StringType *>(type)) {
    llvm::Value *srcPtr = rhs.addr;

    if (srcPtr == nullptr) {
      auto *stringTy = getType(type);
      srcPtr = createEntryAlloca(ctx.func, stringTy, "string.assign.src");
      builder.CreateStore(rhs.value, srcPtr);
    }

    lowerStringAssign(getStringSuffix(type), dst.dst, srcPtr, rhs.category);
    return;
  }

  if (type->kind == TypeSymbol::TypeKind::STRUCT) {
    lowerStructAssign(type, dst.dst, rhs, rhs.category, ctx);
    return;
  }

  if (type->kind == TypeSymbol::TypeKind::ENUM) {

    if (rhs.category == MIRValueCategory::OwnedTemp) {
      lowerEnumMoveAssign(type, dst.dst, rhs.addr);
      return;
    }

    lowerEnumCopyAssign(type, dst.dst, rhs.addr, ctx);

    return;
  }
  if (rhs.value == nullptr) {
    if (rhs.addr == nullptr) {
      Error::internal("assign rhs has neither value nor address");
    }

    rhs.value = builder.CreateLoad(getType(type), rhs.addr);
  }
  builder.CreateStore(rhs.value, dst.dst);
}

void llvmCodegen::lowerEnumMoveAssign(TypeSymbol *type, llvm::Value *dst,
                                      llvm::Value *src) {
  if (dst == src) {
    return;
  }

  auto *layoutTy = getLayoutType(type);

  builder.CreateCall(defaultDestroys.at(type), {dst});

  auto *value = builder.CreateLoad(layoutTy, src);
  builder.CreateStore(value, dst);

  auto *empty =
      llvm::ConstantAggregateZero::get(llvm::cast<llvm::StructType>(layoutTy));

  builder.CreateStore(empty, src);
}

void llvmCodegen::lowerEnumCopyAssign(TypeSymbol *type, llvm::Value *dst,
                                      llvm::Value *src, FuncContext &ctx) {
  if (dst == src) {
    return;
  }

  auto *layoutTy = getLayoutType(type);
  auto *ptrTy = builder.getPtrTy();

  // 기존 dst의 payload 제거
  builder.CreateCall(defaultDestroys.at(type), {dst});

  auto *srcTagPtr = builder.CreateStructGEP(layoutTy, src, 0);
  auto *srcTag = builder.CreateLoad(builder.getInt32Ty(), srcTagPtr);

  auto *srcPayloadSlot = builder.CreateStructGEP(layoutTy, src, 1);
  auto *srcPayload =
      builder.CreateLoad(ptrTy, srcPayloadSlot, "enum.copy.src.payload");

  auto *dstTagPtr = builder.CreateStructGEP(layoutTy, dst, 0);
  auto *dstPayloadSlot = builder.CreateStructGEP(layoutTy, dst, 1);

  builder.CreateStore(srcTag, dstTagPtr);

  auto *doneBB = llvm::BasicBlock::Create(context, "enum.copy.done", ctx.func);

  auto *defaultBB =
      llvm::BasicBlock::Create(context, "enum.copy.unit", ctx.func);

  auto *sw = builder.CreateSwitch(srcTag, defaultBB);

  for (auto &variant : type->variants) {
    if (variant->payloadType == nullptr) {
      continue;
    }

    auto *copyBB = llvm::BasicBlock::Create(
        context, "enum.copy." + variant->name, ctx.func);

    sw->addCase(builder.getInt32(variant->ordinal), copyBB);

    builder.SetInsertPoint(copyBB);

    auto *payloadTy = variant->payloadType;
    auto *payloadLayoutTy = getType(payloadTy);

    // malloc(sizeof(payload))
    auto *size = llvm::ConstantExpr::getSizeOf(payloadLayoutTy);

    auto *mallocTy =
        llvm::FunctionType::get(ptrTy, {builder.getInt64Ty()}, false);

    auto *newPayload = builder.CreateCall(getRuntimeFunc("malloc", mallocTy),
                                          {size}, "enum.copy.payload");

    // assign()이 기존 dst를 destroy하므로 반드시 zero-init
    builder.CreateStore(llvm::Constant::getNullValue(payloadLayoutTy),
                        newPayload);

    LoweredValue payloadRhs;
    payloadRhs.addr = srcPayload;
    payloadRhs.category = MIRValueCategory::Borrowed;

    if (payloadTy->kind != TypeSymbol::TypeKind::STRUCT &&
        dynamic_cast<StringType *>(payloadTy) == nullptr) {
      payloadRhs.value = builder.CreateLoad(payloadLayoutTy, srcPayload);
    }

    assign({newPayload, payloadTy}, payloadRhs, payloadTy, ctx);

    builder.CreateStore(newPayload, dstPayloadSlot);
    builder.CreateBr(doneBB);
  }

  builder.SetInsertPoint(defaultBB);

  // unit variant
  builder.CreateStore(
      llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptrTy)),
      dstPayloadSlot);

  builder.CreateBr(doneBB);

  builder.SetInsertPoint(doneBB);
}

void llvmCodegen::lowerStructAssign(TypeSymbol *ty, llvm::Value *dst,
                                    LoweredValue rhs, MIRValueCategory category,
                                    FuncContext &ctx) {
  auto *layoutTy = getLayoutType(ty);

  llvm::Value *src = rhs.addr;

  if (src == nullptr) {
    // createTempAlloca(layoutTy, "struct.assign.tmp")
    src = createEntryAlloca(ctx.func, layoutTy, "struct.assign.tmp");
    builder.CreateStore(rhs.value, src);
  }

  for (auto *field : ty->fields) {
    auto *fieldTy = field->typeSymbol;

    auto *dstField =
        builder.CreateStructGEP(layoutTy, dst, field->index, field->name);

    auto *srcField =
        builder.CreateStructGEP(layoutTy, src, field->index, field->name);

    LoweredValue fieldRhs;
    fieldRhs.addr = srcField;
    fieldRhs.category = category;
    assign({dstField, fieldTy}, fieldRhs, fieldTy, ctx);
  }

  if (category == MIRValueCategory::OwnedTemp && rhs.addr != nullptr) {
    ctx.canceledCleanups.insert(rhs.addr); // 있으면
  }
}

void llvmCodegen::lowerStringCopyAssign(Str type, llvm::Value *dst,
                                        llvm::Value *srcPtr) {
  auto *ptrTy = llvm::PointerType::getUnqual(context);
  auto *voidTy = builder.getVoidTy();

  auto *copyTy = llvm::FunctionType::get(voidTy, {ptrTy, ptrTy}, false);
  builder.CreateCall(getRuntimeFunc("hrd_copy_" + type, copyTy), {dst, srcPtr});
}

void llvmCodegen::lowerStringMoveAssign(Str type, llvm::Value *dst,
                                        llvm::Value *srcPtr) {
  auto *s8Ty = getType(table.registry.getBuilt(type));

  auto *v = builder.CreateLoad(s8Ty, srcPtr);
  builder.CreateStore(v, dst);

  // src 비우기: temp cleanup/destroy가 원본 버퍼를 다시 free하지 않게
  auto *empty =
      llvm::ConstantAggregateZero::get(llvm::cast<llvm::StructType>(s8Ty));
  builder.CreateStore(empty, srcPtr);
}

void llvmCodegen::lowerStringAssign(Str type, llvm::Value *dst,
                                    llvm::Value *srcPtr,
                                    MIRValueCategory category) {
  if (dst == srcPtr) {
    return;
  }
  auto *ptrTy = llvm::PointerType::getUnqual(context);
  auto *voidTy = builder.getVoidTy();

  auto *destroyTy = llvm::FunctionType::get(voidTy, {ptrTy}, false);
  builder.CreateCall(getRuntimeFunc("hrd_destroy_" + type, destroyTy), {dst});

  if (category == MIRValueCategory::OwnedTemp) {
    lowerStringMoveAssign(type, dst, srcPtr);
  } else {
    lowerStringCopyAssign(type, dst, srcPtr);
  }
}

void llvmCodegen::lowerLocalDecl(MIRLocalDeclStmt *stmt, FuncContext &ctx) {
  llvm::Type *ty = getType(stmt->type);
  llvm::AllocaInst *slot = createEntryAlloca(ctx.func, ty, stmt->symbol->name);
  ctx.locals.emplace(stmt->symbol, slot);

  if (needsDestroy(stmt->type)) {
    builder.CreateStore(llvm::Constant::getNullValue(ty), slot);
    addClean(slot, stmt->type, ctx);
  }

  if (stmt->init) {
    auto init = lowerValue(stmt->init.get(), ctx);

    assign({slot, stmt->type}, init, stmt->init->type, ctx);
  }
}
