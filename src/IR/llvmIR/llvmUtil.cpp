#include "hrd/IR/llvmIR/llvmCodegen.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/util/Error.h"
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

llvm::Value *llvmCodegen::castTo(llvm::Value *value, TypeSymbol *sourceType,
                                 TypeSymbol *targetType) {
  llvm::Type *src = value->getType();
  llvm::Type *dst = getType(targetType);

  if (src == dst)
    return value;

  // int -> int
  if (src->isIntegerTy() && dst->isIntegerTy()) {
    unsigned srcBits = src->getIntegerBitWidth();
    unsigned dstBits = dst->getIntegerBitWidth();

    if (srcBits < dstBits) {
      if (isUnsigned(sourceType))
        return builder.CreateZExt(value, dst, "zexttmp");
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

    if (srcBits < dstBits)
      return builder.CreateFPExt(value, dst, "fpexttmp");

    if (srcBits > dstBits)
      return builder.CreateFPTrunc(value, dst, "fptrunctmp");

    return value;
  }

  // int -> float
  if (src->isIntegerTy() && dst->isFloatingPointTy()) {
    if (isUnsigned(sourceType))
      return builder.CreateUIToFP(value, dst, "uitofptmp");

    return builder.CreateSIToFP(value, dst, "sitofptmp");
  }

  // float -> int
  if (src->isFloatingPointTy() && dst->isIntegerTy()) {
    if (isUnsigned(targetType))
      return builder.CreateFPToUI(value, dst, "fptouitmp");

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
