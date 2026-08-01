#include "hrd/IR/llvmIR/llvmCodegen.h"
#include "hrd/SemanticAnalyzer/ResolvedLit.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"

LoweredValue llvmCodegen::lowerStringLiteral(const StringPayload &payload,
                                             TypeSymbol *type) {
  if (!isString(type)) {
    Error::internal("illegal type expect string");
  }

  auto str = dynamic_cast<StringType *>(type);

  if (str->builtinType == BuiltInType::S8) {
    return lowerS8(payload, str);
  }

  if (str->builtinType == BuiltInType::S16) {
    return lowerS16(payload, str);
  }

  if (str->builtinType == BuiltInType::S32) {
    return lowerS32(payload, str);
  }

  Error::internal("invaild");

  /*auto *s8Ty = getLayoutType(type);
  auto *ptrTy = llvm::PointerType::getUnqual(context);
  auto *i64Ty = builder.getInt64Ty();
  auto *voidTy = builder.getVoidTy();

  std::string bytes;
  bytes.reserve(payload.codePoints.size());

  for (uint32_t cp : payload.codePoints) {
    if (cp > 0x7F) {
      Error::internal("non-ascii string literal in s8 lowering");
    }
    bytes.push_back(static_cast<char>(cp));
  }

  auto *out = builder.CreateAlloca(s8Ty, nullptr, "s8.lit.out");
  auto *dataPtr = builder.CreateGlobalString(bytes, "s8lit");
  auto *len =
      llvm::ConstantInt::get(i64Ty, static_cast<uint64_t>(bytes.size()));

  auto *fnTy = llvm::FunctionType::get(voidTy, {ptrTy, ptrTy, i64Ty}, false);

  auto *fn = getRuntimeFunc("hrd_s8_from_literal", fnTy);

  builder.CreateCall(fn, {out, dataPtr, len});

  return {builder.CreateLoad(s8Ty, out, "s8.literal"), out,
          MIRValueCategory::OwnedTemp};*/
}

LoweredValue llvmCodegen::lowerS8(const StringPayload &payload,
                                  StringType *type) {
  auto *stringTy = getLayoutType(type);
  auto *ptrTy = llvm::PointerType::getUnqual(context);
  auto *i64Ty = builder.getInt64Ty();
  auto *voidTy = builder.getVoidTy();

  std::vector<uint8_t> units;
  units.reserve(payload.codePoints.size());

  for (const uint32_t cp : payload.codePoints) {
    if (cp > 0x7F) {
      Error::internal("non-ASCII code point in s8 literal");
    }

    units.push_back(static_cast<uint8_t>(cp));
  }

  auto *out = builder.CreateAlloca(stringTy, nullptr, "s8.lit.out");

  llvm::Value *dataPtr = nullptr;

  if (units.empty()) {
    dataPtr =
        llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(context));
  } else {
    auto *arrayTy = llvm::ArrayType::get(builder.getInt8Ty(), units.size());

    std::vector<llvm::Constant *> values;
    values.reserve(units.size());

    for (uint8_t unit : units) {
      values.push_back(builder.getInt8(unit));
    }

    auto *initializer = llvm::ConstantArray::get(arrayTy, values);

    auto *global = new llvm::GlobalVariable(*llvmModule, arrayTy, true,
                                            llvm::GlobalValue::PrivateLinkage,
                                            initializer, "s8lit");

    global->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
    global->setAlignment(llvm::Align(1));

    dataPtr = builder.CreateInBoundsGEP(
        arrayTy, global, {builder.getInt64(0), builder.getInt64(0)},
        "s8.lit.data");
  }

  auto *len =
      llvm::ConstantInt::get(i64Ty, static_cast<uint64_t>(units.size()));

  auto *fnTy = llvm::FunctionType::get(voidTy, {ptrTy, ptrTy, i64Ty}, false);

  auto *fn = getRuntimeFunc("hrd_s8_from_literal", fnTy);

  builder.CreateCall(fn, {out, dataPtr, len});

  return {
      builder.CreateLoad(stringTy, out, "s8.literal"),
      out,
      MIRValueCategory::OwnedTemp,
  };
}

LoweredValue llvmCodegen::lowerS16(const StringPayload &payload,
                                   StringType *type) {
  auto *stringTy = getLayoutType(type);
  auto *ptrTy = llvm::PointerType::getUnqual(context);
  auto *i16Ty = builder.getInt16Ty();
  auto *i64Ty = builder.getInt64Ty();
  auto *voidTy = builder.getVoidTy();

  std::vector<uint16_t> units;
  units.reserve(payload.codePoints.size());

  for (const uint32_t cp : payload.codePoints) {
    if (cp > 0xFFFF) {
      Error::internal("code point does not fit in s16 literal");
    }

    // Resolver의 UTF-8 decoder에서 surrogate는 이미 거부되어야 한다.
    if (cp >= 0xD800 && cp <= 0xDFFF) {
      Error::internal("surrogate code point in s16 literal");
    }

    units.push_back(static_cast<uint16_t>(cp));
  }

  auto *out = builder.CreateAlloca(stringTy, nullptr, "s16.lit.out");

  llvm::Value *dataPtr = nullptr;

  if (units.empty()) {
    dataPtr =
        llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(context));
  } else {
    auto *arrayTy = llvm::ArrayType::get(i16Ty, units.size());

    std::vector<llvm::Constant *> values;
    values.reserve(units.size());

    for (const uint16_t unit : units) {
      values.push_back(llvm::ConstantInt::get(i16Ty, unit));
    }

    auto *initializer = llvm::ConstantArray::get(arrayTy, values);

    auto *global = new llvm::GlobalVariable(*llvmModule, arrayTy, true,
                                            llvm::GlobalValue::PrivateLinkage,
                                            initializer, "s16lit");

    global->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
    global->setAlignment(llvm::Align(2));

    dataPtr = builder.CreateInBoundsGEP(
        arrayTy, global, {builder.getInt64(0), builder.getInt64(0)},
        "s16.lit.data");
  }

  auto *len =
      llvm::ConstantInt::get(i64Ty, static_cast<uint64_t>(units.size()));

  auto *fnTy = llvm::FunctionType::get(voidTy, {ptrTy, ptrTy, i64Ty}, false);

  auto *fn = getRuntimeFunc("hrd_s16_from_literal", fnTy);

  builder.CreateCall(fn, {out, dataPtr, len});

  return {
      builder.CreateLoad(stringTy, out, "s16.literal"),
      out,
      MIRValueCategory::OwnedTemp,
  };
}

LoweredValue llvmCodegen::lowerS32(const StringPayload &payload,
                                   StringType *type) {
  auto *stringTy = getLayoutType(type);
  auto *ptrTy = llvm::PointerType::getUnqual(context);
  auto *i32Ty = builder.getInt32Ty();
  auto *i64Ty = builder.getInt64Ty();
  auto *voidTy = builder.getVoidTy();

  std::vector<uint32_t> units;
  units.reserve(payload.codePoints.size());

  for (const uint32_t cp : payload.codePoints) {
    if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
      Error::internal("invalid Unicode scalar value in s32 literal");
    }

    units.push_back(cp);
  }

  auto *out = builder.CreateAlloca(stringTy, nullptr, "s32.lit.out");

  llvm::Value *dataPtr = nullptr;

  if (units.empty()) {
    dataPtr =
        llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(context));
  } else {
    auto *arrayTy = llvm::ArrayType::get(i32Ty, units.size());

    std::vector<llvm::Constant *> values;
    values.reserve(units.size());

    for (const uint32_t unit : units) {
      values.push_back(llvm::ConstantInt::get(i32Ty, unit));
    }

    auto *initializer = llvm::ConstantArray::get(arrayTy, values);

    auto *global = new llvm::GlobalVariable(*llvmModule, arrayTy, true,
                                            llvm::GlobalValue::PrivateLinkage,
                                            initializer, "s32lit");

    global->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
    global->setAlignment(llvm::Align(4));

    dataPtr = builder.CreateInBoundsGEP(
        arrayTy, global, {builder.getInt64(0), builder.getInt64(0)},
        "s32.lit.data");
  }

  auto *len =
      llvm::ConstantInt::get(i64Ty, static_cast<uint64_t>(units.size()));

  auto *fnTy = llvm::FunctionType::get(voidTy, {ptrTy, ptrTy, i64Ty}, false);

  auto *fn = getRuntimeFunc("hrd_s32_from_literal", fnTy);

  builder.CreateCall(fn, {out, dataPtr, len});

  return {
      builder.CreateLoad(stringTy, out, "s32.literal"),
      out,
      MIRValueCategory::OwnedTemp,
  };
}