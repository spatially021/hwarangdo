#include "hrd/IR/MIR/MIRExpr.h"
#include "hrd/IR/llvmIR/llvmCodegen.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/util/Error.h"

#include <llvm/IR/Value.h>

void llvmCodegen::assign(LoweredPlace dst, LoweredValue rhs, TypeSymbol *type,
                         FuncContext &ctx) {

  auto *resolvedType = resolveType(type, ctx);
  auto *resolvedDstType = resolveType(dst.type, ctx);

  if (auto *arr = dynamic_cast<ArrayTypeSymbol *>(resolvedDstType)) {

    auto *baseType = resolveType(arr->baseType, ctx);

    if (baseType == resolvedType) {

      if (arr->sizeValue.getActiveBits() > sizeof(uint64_t) * 8) {
        Error::internal("too big arr size");
      }

      const uint64_t size = arr->sizeValue.getZExtValue();

      LoweredValue elementRhs = rhs;

      if (needsDestroy(baseType)) {
        elementRhs.category = MIRValueCategory::Borrowed;
      }

      auto *arrayTy = getType(resolvedDstType);

      for (uint64_t i = 0; i < size; ++i) {

        auto *place = builder.CreateInBoundsGEP(
            arrayTy, dst.dst, {builder.getInt32(0), builder.getInt64(i)});

        assign({place, baseType}, elementRhs, baseType, ctx);
      }

      return;
    }
  }

  if (dynamic_cast<StringType *>(resolvedType)) {

    llvm::Value *srcPtr = rhs.addr;

    if (srcPtr == nullptr) {

      auto *stringTy = getType(resolvedType);

      srcPtr = createEntryAlloca(ctx.func, stringTy, "string.assign.src");

      builder.CreateStore(rhs.value, srcPtr);
    }

    lowerStringAssign(getStringSuffix(resolvedType), dst.dst, srcPtr,
                      rhs.category);

    return;
  }

  if (resolvedType->kind == TypeKind::STRUCT) {

    lowerStructAssign(resolvedType, dst.dst, rhs, rhs.category, ctx);

    return;
  }

  if (resolvedType->kind == TypeKind::ENUM) {

    if (rhs.category == MIRValueCategory::OwnedTemp) {

      lowerEnumMoveAssign(resolvedType, dst.dst, rhs.addr);

      return;
    }

    lowerEnumCopyAssign(resolvedType, dst.dst, rhs.addr, ctx);

    return;
  }

  if (rhs.value == nullptr) {

    if (rhs.addr == nullptr) {
      Error::internal("assign rhs has neither value nor address");
    }

    rhs.value = builder.CreateLoad(getType(resolvedType), rhs.addr);
  }

  builder.CreateStore(rhs.value, dst.dst);
}

void llvmCodegen::lowerEnumMoveAssign(TypeSymbol *type, llvm::Value *dst,
                                      llvm::Value *src) {

  if (dst == src) {
    return;
  }

  // assign()에서 이미 concrete type으로 resolve되어 들어온다.
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

  auto *resolvedType = resolveType(type, ctx);
  auto *layoutTy = getLayoutType(resolvedType);

  auto *ptrTy = builder.getPtrTy();

  // 기존 dst의 payload 제거
  builder.CreateCall(defaultDestroys.at(resolvedType), {dst});

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

  auto *en = dyn_cast<EnumType>(resolvedType);

  for (auto &variant : en->variants) {

    if (variant->payloadType == nullptr) {
      continue;
    }

    auto *copyBB = llvm::BasicBlock::Create(
        context, "enum.copy." + variant->name, ctx.func);

    sw->addCase(builder.getInt32(variant->ordinal), copyBB);

    builder.SetInsertPoint(copyBB);

    auto *payloadTy = resolveType(variant->payloadType, ctx);

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

    if (payloadTy->kind != TypeKind::STRUCT &&
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

  auto *resolvedType = resolveType(ty, ctx);
  auto *layoutTy = getLayoutType(resolvedType);

  llvm::Value *src = rhs.addr;

  if (src == nullptr) {

    src = createEntryAlloca(ctx.func, layoutTy, "struct.assign.tmp");

    builder.CreateStore(rhs.value, src);
  }

  /*
   * GenericSymbol이 ObjectType 자체가 아니라 origin을 가리키는 구조라면
   * 여기서 origin으로 내려가야 함.
   */
  TypeSymbol *objectType = resolvedType;

  if (auto *generic = dynamic_cast<GenericSymbol *>(resolvedType)) {

    objectType = generic->origin;
  }

  auto *obj = dyn_cast<ObjectType>(objectType);

  if (obj == nullptr) {
    Error::internal("struct type has no object layout: " + resolvedType->name);
  }

  for (auto *field : obj->fields) {

    auto *fieldTy = resolveType(field->typeSymbol, ctx);

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

    ctx.canceledCleanups.insert(rhs.addr);
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

  auto *resolvedType = resolveType(stmt->type, ctx);

  llvm::Type *ty = getType(resolvedType);

  llvm::AllocaInst *slot = createEntryAlloca(ctx.func, ty, stmt->symbol->name);

  ctx.locals.emplace(stmt->symbol, slot);

  if (needsDestroy(resolvedType)) {

    builder.CreateStore(llvm::Constant::getNullValue(ty), slot);

    addClean(slot, resolvedType, ctx);
  }

  if (stmt->init) {

    auto init = lowerValue(stmt->init.get(), ctx);

    auto *initType = resolveType(stmt->init->type, ctx);

    assign({slot, resolvedType}, init, initType, ctx);
  }
}