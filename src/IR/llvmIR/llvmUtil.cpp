#include "hrd/IR/llvmIR/llvmCodegen.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/util/Error.h"
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

llvm::Value *llvmCodegen::castTo(llvm::Value *value, TypeSymbol *sourceType,
                                 TypeSymbol *targetType) {
  llvm::Type *src = value->getType();
  llvm::Type *dst = getType(targetType);

  if (src == dst) {
    return value;
  }

  // string / struct / class layout value 등 aggregate 타입.
  // 동일한 언어 타입이면 LLVM 타입 포인터가 달라도 bitcast가 아니라 그대로 허용
  // 가능한지 확인한다.
  if (sourceType == targetType) {
    if (src->isStructTy() && dst->isStructTy()) {
      return value;
    }

    if (src->isPointerTy() && dst->isPointerTy()) {
      return builder.CreateBitCast(value, dst, "ptrcasttmp");
    }
  }

  // aggregate는 암묵 캐스팅 대상이 아니다.
  // 단, 여기까지 왔다는 건 서로 다른 value/class/struct/string 타입을
  // 억지로 cast하려는 상황이므로 오류.
  if (src->isStructTy() || dst->isStructTy()) {
    Error::internal("invalid aggregate cast");
  }

  // int -> int
  if (src->isIntegerTy() && dst->isIntegerTy()) {
    unsigned srcBits = src->getIntegerBitWidth();
    unsigned dstBits = dst->getIntegerBitWidth();

    if (srcBits < dstBits) {
      if (isUnsigned(sourceType)) {
        return builder.CreateZExt(value, dst, "zexttmp");
      }
      return builder.CreateSExt(value, dst, "sexttmp");
    }

    if (srcBits > dstBits) {
      return builder.CreateTrunc(value, dst, "trunctmp");
    }

    return value;
  }

  // float -> float
  if (src->isFloatingPointTy() && dst->isFloatingPointTy()) {
    uint64_t srcBits = src->getPrimitiveSizeInBits();
    uint64_t dstBits = dst->getPrimitiveSizeInBits();

    if (srcBits < dstBits) {
      return builder.CreateFPExt(value, dst, "fpexttmp");
    }

    if (srcBits > dstBits) {
      return builder.CreateFPTrunc(value, dst, "fptrunctmp");
    }

    return value;
  }

  // int -> float
  if (src->isIntegerTy() && dst->isFloatingPointTy()) {
    if (isUnsigned(sourceType)) {
      return builder.CreateUIToFP(value, dst, "uitofptmp");
    }

    return builder.CreateSIToFP(value, dst, "sitofptmp");
  }

  // float -> int
  if (src->isFloatingPointTy() && dst->isIntegerTy()) {
    if (isUnsigned(targetType)) {
      return builder.CreateFPToUI(value, dst, "fptouitmp");
    }

    return builder.CreateFPToSI(value, dst, "fptositmp");
  }

  Error::internal("invalid implicit cast");
}

bool llvmCodegen::isUnsigned(TypeSymbol *type) {
  auto integer = dynamic_cast<IntType *>(type);
  if (integer == nullptr)
    return false;
  return !integer->isSigned;
}

bool llvmCodegen::isFloat(TypeSymbol *type) {
  return dynamic_cast<FloatType *>(type);
}

bool llvmCodegen::isInt(TypeSymbol *type) {
  return dynamic_cast<IntType *>(type);
}

bool llvmCodegen::isString(TypeSymbol *type) {
  return dynamic_cast<StringType *>(type);
}

llvm::Value *llvmCodegen::getSizeOf(llvm::Type *type) {
  auto *nullPtr =
      llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0));
  llvm::Value *gep = builder.CreateGEP(
      type, nullPtr,
      llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 1));

  return builder.CreatePtrToInt(gep, llvm::Type::getInt64Ty(context));
}

llvm::FunctionCallee llvmCodegen::getOrDeclareMalloc() {
  auto *i64Ty = llvm::Type::getInt64Ty(context);
  auto *ptrTy = llvm::PointerType::getUnqual(context);

  return llvmModule->getOrInsertFunction(
      "malloc", llvm::FunctionType::get(ptrTy, {i64Ty}, false));
}

llvm::Type *llvmCodegen::getType(TypeSymbol *t) {

  if (auto g = dynamic_cast<GenericSymbol *>(t)) {
    if (dynamic_cast<HandleSymbol *>(g->origin)) {
      return getHrdHandleType();
    }
  }

  if (t->kind == TypeSymbol::TypeKind::CLASS) {
    return llvm::PointerType::getUnqual(context);
  }

  return getLayoutType(t);
}

llvm::Type *llvmCodegen::getLayoutType(TypeSymbol *t) {
  if (auto it = types.find(t); it != types.end()) {
    return it->second;
  }

  if (auto it = enums.find(t); it != enums.end()) {
    return it->second;
  }

  if (auto *arr = dynamic_cast<ArrayTypeSymbol *>(t)) {
    auto *elemTy = getType(arr->baseType);
    auto len = arrayLengthToU64(arr->sizeValue);
    auto *arrTy = llvm::ArrayType::get(elemTy, len);
    types.emplace(t, arrTy);
    return arrTy;
  }

  Error::internal("llvm type not declared: " + t->name);
}

llvm::Type *llvmCodegen::getFieldType(TypeSymbol *type) {
  if (type->kind == TypeSymbol::TypeKind::CLASS) {
    Error::internal("class observer cannot be stored as field");
  }

  return getType(type);
}

bool llvmCodegen::needsDestroy(TypeSymbol *type) {

  if (auto arr = dynamic_cast<ArrayTypeSymbol *>(type)) {
    return needsDestroy(arr->baseType);
  }

  return isString(type) || type->kind == TypeSymbol::TypeKind::STRUCT ||
         type->kind == TypeSymbol::TypeKind::ENUM;
}

llvm::Type *llvmCodegen::getHrdHandleType() {
  return llvm::Type::getInt64Ty(context);
}

std::string llvmCodegen::getStringSuffix(TypeSymbol *type) {
  if (type == table.getBuilt("s8")) {
    return "s8";
  }

  if (type == table.getBuilt("s16")) {
    return "s16";
  }

  if (type == table.getBuilt("s32")) {
    return "s32";
  }

  Error::internal("invalid string type");
}
