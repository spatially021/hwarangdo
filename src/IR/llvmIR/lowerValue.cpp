#include "hrd/IR/MIR/MIRExpr.h"
#include "hrd/IR/llvmIR/llvmCodegen.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/enums/Operator.h"
#include "hrd/util/Error.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>
#include <memory>

static bool isCompare(Operator op) {
  switch (op) {
  case Operator::EQ:
  case Operator::NT:
  case Operator::LS:
  case Operator::LSE:
  case Operator::GR:
  case Operator::GRE:
    return true;

  case Operator::ADD:
  case Operator::SUB:
  case Operator::MUL:
  case Operator::DIV:
  case Operator::REM:
  case Operator::POW:
  case Operator::B_AND:
  case Operator::B_OR:
  case Operator::B_XOR:
  case Operator::AND:
  case Operator::OR:
  case Operator::LSH:
  case Operator::RSH:
  case Operator::L_NOT:
  case Operator::B_NOT:
  case Operator::PLUS:
  case Operator::MINUS:
    return false;
  }
  return false;
}

LoweredValue llvmCodegen::lowerValue(MIRValue *value, FuncContext &ctx) {
  if (auto *v = dynamic_cast<MIRBinaryExpr *>(value)) {
    return lowerBinaryExpr(v, ctx);
  }

  if (auto *v = dynamic_cast<MIRLoad *>(value)) {
    return lowerLoad(v, ctx);
  }

  if (auto *v = dynamic_cast<MIRPayloadExtractExpr *>(value)) {
    return lowerPayloadExtractExpr(v, ctx);
  }

  if (auto *v = dynamic_cast<MIRLiteralExpr *>(value)) {
    return lowerLiteralExpr(v, ctx);
  }

  if (auto *v = dynamic_cast<MIRArrayInitExpr *>(value)) {
    return lowerArrayInitExpr(v, ctx);
  }

  if (auto *v = dynamic_cast<MIRUnaryExpr *>(value)) {
    return lowerUnaryExpr(v, ctx);
  }

  if (auto *v = dynamic_cast<MIRCastExpr *>(value)) {
    return lowerCastExpr(v, ctx);
  }

  if (auto *v = dynamic_cast<MIRCallExpr *>(value)) {
    return lowerCallExpr(v, ctx);
  }

  if (auto *v = dynamic_cast<MIRSpawnExpr *>(value)) {
    return lowerSpawnExpr(v, ctx);
  }

  if (auto *v = dynamic_cast<MIRViewExpr *>(value)) {
    return lowerViewExpr(v, ctx);
  }

  if (auto *v = dynamic_cast<MIRStructInitExpr *>(value)) {
    return lowerStructInitExpr(v, ctx);
  }

  if (auto *v = dynamic_cast<MIRVariantExpr *>(value)) {
    return lowerVariantExpr(v, ctx);
  }

  if (auto *v = dynamic_cast<MIRRuntimeCallExpr *>(value)) {
    return lowerRuntime(v, ctx);
  }

  Error::internal("unknown MIRValue in LLVM lowering");
}

LoweredValue llvmCodegen::lowerBinaryExpr(MIRBinaryExpr *expr,
                                          FuncContext &ctx) {
  if (expr->op == Operator::AND) {
    return lowerLogicalAnd(expr, ctx);
  }

  if (expr->op == Operator::OR) {
    return lowerLogicalOr(expr, ctx);
  }

  auto type = isCompare(expr->op) ? expr->operandType : expr->type;
  auto lhsRaw = lowerValue(expr->lhs.get(), ctx);
  LoweredValue lhs = castTo(lhsRaw, expr->lhs->type, type);

  auto rawRhs = lowerValue(expr->rhs.get(), ctx);

  bool usePowi = expr->op == Operator::POW && isInt(expr->rhs->type) &&
                 isFloat(expr->type);

  LoweredValue rhs =
      usePowi ? castTo(rawRhs, expr->rhs->type, table.registry.getBuilt("i32"))
              : castTo(rawRhs, expr->rhs->type, type);
  switch (expr->op) {

  case Operator::ADD:
    if (isString(type)) {
      auto *sTy = getType(expr->type);
      auto name = expr->type->name;
      auto *ptrTy = llvm::PointerType::getUnqual(context);
      auto *voidTy = builder.getVoidTy();
      auto *out = builder.CreateAlloca(sTy, nullptr, name + ".add.out");
      auto *lhsPtr = lhsRaw.addr;
      if (!lhsPtr) {
        lhsPtr = builder.CreateAlloca(sTy, nullptr, name + ".add.lhs");
        builder.CreateStore(lhsRaw.value, lhsPtr);
      }

      auto *rhsPtr = rhs.addr;
      if (!rhsPtr) {
        rhsPtr = builder.CreateAlloca(sTy, nullptr, name + ".add.rhs");
        builder.CreateStore(rhs.value, rhsPtr);
      }

      auto *fnTy =
          llvm::FunctionType::get(voidTy, {ptrTy, ptrTy, ptrTy}, false);
      builder.CreateCall(getRuntimeFunc("hrd_add_" + name, fnTy),
                         {out, lhsPtr, rhsPtr});

      ctx.cleanupStack.push_back({out, type});

      return {builder.CreateLoad(sTy, out, name + ".add"), out,
              expr->valueCategory};
    }

    return {isFloat(type) ? builder.CreateFAdd(lhs.value, rhs.value)
                          : builder.CreateAdd(lhs.value, rhs.value)};
  case Operator::SUB:
    return {isFloat(type) ? builder.CreateFSub(lhs.value, rhs.value)
                          : builder.CreateSub(lhs.value, rhs.value)};

  case Operator::MUL:
    return {isFloat(type) ? builder.CreateFMul(lhs.value, rhs.value)
                          : builder.CreateMul(lhs.value, rhs.value)};

  case Operator::DIV: {
    if (isFloat(type)) {
      return {builder.CreateFDiv(lhs.value, rhs.value)};
    }
    if (isUnsigned(type)) {
      return {builder.CreateUDiv(lhs.value, rhs.value)};
    }
    return {builder.CreateSDiv(lhs.value, rhs.value)};
  }
  case Operator::REM: {
    if (isFloat(type)) {
      return {builder.CreateFRem(lhs.value, rhs.value)};
    }
    if (isUnsigned(type)) {
      return {builder.CreateURem(lhs.value, rhs.value)};
    }
    return {builder.CreateSRem(lhs.value, rhs.value)};
  }
  case Operator::POW: {
    if (isInt(expr->lhs->type) && isInt(expr->rhs->type)) {
      Error::internal("not developed int ** int");
    }

    if (usePowi) {
      auto pow = llvm::Intrinsic::getOrInsertDeclaration(
          llvmModule.get(), llvm::Intrinsic::powi, {getType(expr->type)});

      return {builder.CreateCall(pow, {lhs.value, rhs.value})};
    }

    auto pow = llvm::Intrinsic::getOrInsertDeclaration(
        llvmModule.get(), llvm::Intrinsic::pow, {getType(expr->type)});

    return {builder.CreateCall(pow, {lhs.value, rhs.value})};
  }
  case Operator::B_AND: {
    return {builder.CreateAnd(lhs.value, rhs.value)};
  }
  case Operator::B_OR:
    return {builder.CreateOr(lhs.value, rhs.value)};
  case Operator::B_XOR:
    return {builder.CreateXor(lhs.value, rhs.value)};
  case Operator::EQ: {
    if (dynamic_cast<StringType *>(type)) {
      auto *strTy = getLayoutType(type); // %string8 = { ptr, i64, i64 }
      auto *ptrTy = llvm::PointerType::getUnqual(context);

      auto *callTy =
          llvm::FunctionType::get(builder.getInt1Ty(), {ptrTy, ptrTy}, false);

      auto *func = getRuntimeFunc("hrd_string_eq_s8", callTy);
      if (!func) {
        Error::internal("missing runtime function: hrd_string_eq_s8");
      }

      auto *lhsAddr = builder.CreateAlloca(strTy, nullptr, "lhs.str.addr");
      auto *rhsAddr = builder.CreateAlloca(strTy, nullptr, "rhs.str.addr");

      builder.CreateStore(lhs.value, lhsAddr);
      builder.CreateStore(rhs.value, rhsAddr);

      return {builder.CreateCall(func, {lhsAddr, rhsAddr}, "s8.eq")};
    }

    if (isFloat(type)) {
      return {builder.CreateFCmpOEQ(lhs.value, rhs.value)};
    }

    return {builder.CreateICmpEQ(lhs.value, rhs.value)};
  }
  case Operator::NT: {
    if (dynamic_cast<StringType *>(type)) {
      auto *strTy = getLayoutType(type); // %string8 = { ptr, i64, i64 }
      auto *ptrTy = llvm::PointerType::getUnqual(context);

      auto *callTy =
          llvm::FunctionType::get(builder.getInt1Ty(), {ptrTy, ptrTy}, false);

      auto *func = getRuntimeFunc("hrd_string_ne_s8", callTy);
      if (!func) {
        Error::internal("missing runtime function: hrd_string_ne_s8");
      }

      auto *lhsAddr = builder.CreateAlloca(strTy, nullptr, "lhs.str.addr");
      auto *rhsAddr = builder.CreateAlloca(strTy, nullptr, "rhs.str.addr");

      builder.CreateStore(lhs.value, lhsAddr);
      builder.CreateStore(rhs.value, rhsAddr);

      return {builder.CreateCall(func, {lhsAddr, rhsAddr}, "s8.ne")};
    }

    if (isFloat(type)) {
      return {builder.CreateFCmpONE(lhs.value, rhs.value)};
    }

    return {builder.CreateICmpNE(lhs.value, rhs.value)};
  }
  case Operator::LS: {
    if (isFloat(type)) {
      return {builder.CreateFCmpOLT(lhs.value, rhs.value)};
    }
    if (isUnsigned(type)) {
      return {builder.CreateICmpULT(lhs.value, rhs.value)};
    }
    return {builder.CreateICmpSLT(lhs.value, rhs.value)};
  }
  case Operator::LSE: {
    if (isFloat(type)) {
      return {builder.CreateFCmpOLE(lhs.value, rhs.value)};
    }
    if (isUnsigned(type)) {
      return {builder.CreateICmpULE(lhs.value, rhs.value)};
    }
    return {builder.CreateICmpSLE(lhs.value, rhs.value)};
  }
  case Operator::GR: {
    if (isFloat(type)) {
      return {builder.CreateFCmpOGT(lhs.value, rhs.value)};
    }
    if (isUnsigned(type)) {
      return {builder.CreateICmpUGT(lhs.value, rhs.value)};
    }
    return {builder.CreateICmpSGT(lhs.value, rhs.value)};
  }
  case Operator::GRE: {
    if (isFloat(type)) {
      return {builder.CreateFCmpOGE(lhs.value, rhs.value)};
    }
    if (isUnsigned(type)) {
      return {builder.CreateICmpUGE(lhs.value, rhs.value)};
    }
    return {builder.CreateICmpSGE(lhs.value, rhs.value)};
  }

  case Operator::LSH: {
    llvm::Value *amount = castTo(rhs, expr->rhs->type, expr->lhs->type).value;
    return {builder.CreateShl(lhs.value, amount)};
  }

  case Operator::RSH: {
    llvm::Value *amount = castTo(rhs, expr->rhs->type, expr->lhs->type).value;

    return {isUnsigned(expr->lhs->type)
                ? builder.CreateLShr(lhs.value, amount)
                : builder.CreateAShr(lhs.value, amount)};
  }

  case Operator::AND:
  case Operator::OR:
    Error::internal("unexpected and/or use");
  case Operator::L_NOT:
  case Operator::B_NOT:
  case Operator::PLUS:
  case Operator::MINUS:
    Error::internal("in binary but unary detected");
  }
  Error::internal("unknwon operator type");
}

LoweredValue llvmCodegen::lowerLogicalAnd(MIRBinaryExpr *expr,
                                          FuncContext &ctx) {
  llvm::Function *func = builder.GetInsertBlock()->getParent();

  llvm::BasicBlock *rhsBlock =
      llvm::BasicBlock::Create(context, "land.rhs", func);
  llvm::BasicBlock *falseBlock =
      llvm::BasicBlock::Create(context, "land.false", func);
  llvm::BasicBlock *mergeBlock =
      llvm::BasicBlock::Create(context, "land.merge", func);

  llvm::Value *lhs = lowerValue(expr->lhs.get(), ctx).value;
  builder.CreateCondBr(lhs, rhsBlock, falseBlock);

  builder.SetInsertPoint(rhsBlock);
  llvm::Value *rhs = lowerValue(expr->rhs.get(), ctx).value;
  builder.CreateBr(mergeBlock);
  rhsBlock = builder.GetInsertBlock();

  builder.SetInsertPoint(falseBlock);
  llvm::Value *falseVal = llvm::ConstantInt::getFalse(context);
  builder.CreateBr(mergeBlock);
  falseBlock = builder.GetInsertBlock();

  builder.SetInsertPoint(mergeBlock);
  llvm::PHINode *phi =
      builder.CreatePHI(llvm::Type::getInt1Ty(context), 2, "land.result");

  phi->addIncoming(rhs, rhsBlock);
  phi->addIncoming(falseVal, falseBlock);

  return {phi};
}

LoweredValue llvmCodegen::lowerLogicalOr(MIRBinaryExpr *expr,
                                         FuncContext &ctx) {
  llvm::Function *func = builder.GetInsertBlock()->getParent();

  llvm::BasicBlock *trueBlock =
      llvm::BasicBlock::Create(context, "lor.true", func);
  llvm::BasicBlock *rhsBlock =
      llvm::BasicBlock::Create(context, "lor.rhs", func);
  llvm::BasicBlock *mergeBlock =
      llvm::BasicBlock::Create(context, "lor.merge", func);

  llvm::Value *lhs = lowerValue(expr->lhs.get(), ctx).value;
  builder.CreateCondBr(lhs, trueBlock, rhsBlock);

  builder.SetInsertPoint(trueBlock);
  llvm::Value *trueVal = llvm::ConstantInt::getTrue(context);
  builder.CreateBr(mergeBlock);
  trueBlock = builder.GetInsertBlock();

  builder.SetInsertPoint(rhsBlock);
  llvm::Value *rhs = lowerValue(expr->rhs.get(), ctx).value;
  builder.CreateBr(mergeBlock);
  rhsBlock = builder.GetInsertBlock();

  builder.SetInsertPoint(mergeBlock);
  llvm::PHINode *phi =
      builder.CreatePHI(llvm::Type::getInt1Ty(context), 2, "lor.result");

  phi->addIncoming(trueVal, trueBlock);
  phi->addIncoming(rhs, rhsBlock);

  return {phi};
}

LoweredValue llvmCodegen::lowerLoad(MIRLoad *expr, FuncContext &ctx) {
  auto ptr = lowerPlace(expr->place.get(), ctx).dst;
  llvm::Type *valueTy = getType(expr->type);
  return {builder.CreateLoad(valueTy, ptr, "loadtmp"), ptr,
          MIRValueCategory::Borrowed};
}

LoweredValue llvmCodegen::lowerPayloadExtractExpr(MIRPayloadExtractExpr *expr,
                                                  FuncContext &ctx) {
  auto *variant = expr->symbol;

  if (!variant || !variant->payloadType) {
    Error::internal("invalid payload extract variant");
  }

  auto *load = dynamic_cast<MIRLoad *>(expr->enumValue.get());
  if (!load) {
    Error::internal("payload extract source must be place load");
  }

  auto *enumType = expr->enumValue->type;

  if (!enumType || enumType->kind != TypeSymbol::TypeKind::ENUM) {
    Error::internal("payload extract source is not enum");
  }

  auto *enumAddr = lowerPlace(load->place.get(), ctx).dst;
  auto *enumLayoutTy = getLayoutType(enumType);

  auto *payloadSlot =
      builder.CreateStructGEP(enumLayoutTy, enumAddr, 1, "payload.slot");

  auto *payloadAddr =
      builder.CreateLoad(builder.getPtrTy(), payloadSlot, "payload.addr");

  auto *payloadTy = getType(variant->payloadType);

  auto *payloadValue =
      builder.CreateLoad(payloadTy, payloadAddr, "payload.value");

  return {
      payloadValue, payloadAddr,
      MIRValueCategory::Borrowed // 네 실제 category 규칙에 맞게
  };
}

LoweredValue llvmCodegen::lowerLiteralExpr(MIRLiteralExpr *expr,
                                           FuncContext &ctx) {
  auto lit = expr->literal;
  llvm::Type *ty = getType(expr->type);

  if (lit.isBool()) {
    return {llvm::ConstantInt::getBool(context, lit.asBool())};
  }

  if (lit.isInt()) {
    auto *intTy = llvm::cast<llvm::IntegerType>(ty);

    auto value = lit.asInt().value;

    if (value.getBitWidth() != intTy->getBitWidth()) {
      value = value.sextOrTrunc(intTy->getBitWidth());
    }

    return {llvm::ConstantInt::get(context, value)};
  }

  if (lit.isFloat()) {
    auto value = lit.asFloat().value;

    const llvm::fltSemantics *semantics = nullptr;

    if (ty->isHalfTy()) {
      semantics = &llvm::APFloat::IEEEhalf();
    } else if (ty->isFloatTy()) {
      semantics = &llvm::APFloat::IEEEsingle();
    } else if (ty->isDoubleTy()) {
      semantics = &llvm::APFloat::IEEEdouble();
    } else if (ty->isFP128Ty()) {
      semantics = &llvm::APFloat::IEEEquad();
    } else {
      Error::internal("invalid float literal LLVM type");
    }

    bool losesInfo = false;
    value.convert(*semantics, llvm::APFloat::rmNearestTiesToEven, &losesInfo);

    return {llvm::ConstantFP::get(context, value)};
  }
  if (lit.isChar()) {
    return {llvm::ConstantInt::get(ty, lit.asChar().codePoint)};
  }

  if (lit.isString()) {
    auto value = lowerStringLiteral(lit.asString(), lit.type);
    ctx.cleanupStack.push_back({value.addr, lit.type});
    return value;
  }

  Error::internal("unknwon literal type");
}

LoweredValue llvmCodegen::lowerUnaryExpr(MIRUnaryExpr *expr, FuncContext &ctx) {
  auto operand = lowerValue(expr->operrand.get(), ctx);
  switch (expr->op) {

  case Operator::L_NOT:
  case Operator::B_NOT:
    return {builder.CreateNot(operand.value)};

  case Operator::PLUS:
    return operand;
  case Operator::MINUS:
    return {isFloat(expr->type) ? builder.CreateFNeg(operand.value)
                                : builder.CreateNeg(operand.value)};

  default:
    Error::internal("expect unary but use binary");
  }
}

LoweredValue llvmCodegen::lowerCastExpr(MIRCastExpr *expr, FuncContext &ctx) {
  auto operand = lowerValue(expr->operrand.get(), ctx);
  return castTo(operand, expr->from, expr->to);
}

LoweredValue llvmCodegen::lowerCallExpr(MIRCallExpr *expr, FuncContext &ctx) {
  auto callee = funcs.at(expr->method);

  std::vector<llvm::Value *> args;

  // self
  if (auto l = dynamic_cast<MIRLoad *>(expr->base.get())) {
    args.push_back(lowerReceiverPtr(l->place.get(), ctx));
  } else {
    Error::internal("fail to get receiver");
  }

  // 일반 인자
  vector<MIRValue *> values;
  for (auto &arg : expr->args) {
    values.push_back(arg.get());
  }
  auto out = lowerArgs(args, values, ctx);

  auto call = builder.CreateCall(callee, args);

  for (auto &o : out) {
    builder.CreateCall(defaultDestroys.at(o.type), {o.addr});
  }

  return {call};
}

LoweredValue llvmCodegen::lowerStructInitExpr(MIRStructInitExpr *expr,
                                              FuncContext &ctx) {
  auto *structTy = getLayoutType(expr->structType); // %Vec2 같은 struct type

  // 1. 임시 struct 공간 생성
  llvm::AllocaInst *tmp =
      createEntryAlloca(ctx.func, structTy, "struct.init.tmp");
  builder.CreateStore(llvm::Constant::getNullValue(structTy), tmp);
  // 2. 인자 준비: self ptr + 일반 args
  std::vector<llvm::Value *> args;
  args.push_back(tmp);
  vector<MIRValue *> values;
  for (auto &arg : expr->args) {
    values.push_back(arg.get());
  }
  auto out = lowerArgs(args, values, ctx);

  auto it = defaultInits.find(expr->structType);
  if (it != defaultInits.end()) {
    auto dInit = it->second;
    builder.CreateCall(dInit, {tmp});
  }

  // 3. init 호출
  if (expr->initMethod != nullptr) {
    auto initFn = getOrgetOrDeclareFunction(expr->initMethod);
    builder.CreateCall(initFn, args);
  }

  auto load = builder.CreateLoad(structTy, tmp, "struct.init.val");
  for (auto &o : out) {
    builder.CreateCall(defaultDestroys.at(o.type), {o.addr});
  }
  // 4. expression value로 반환
  return {load};
}

void llvmCodegen::lowerStructInitTo(MIRStructInitExpr *expr, llvm::Value *dst,
                                    FuncContext &ctx) {
  std::vector<llvm::Value *> args;
  args.push_back(dst);

  for (auto &arg : expr->args) {
    args.push_back(lowerValue(arg.get(), ctx).value);
  }

  if (expr->initMethod != nullptr) {
    builder.CreateCall(funcs.at(expr->initMethod), args);
  }
}
LoweredValue llvmCodegen::lowerVariantExpr(MIRVariantExpr *expr,
                                           FuncContext &ctx) {
  auto *enumType = expr->type;
  auto *enumLayoutTy = getLayoutType(enumType);

  // enum 값 자체는 임시값이므로 스택에 생성
  auto *enumAddr =
      createEntryAlloca(ctx.func, enumLayoutTy, "enum.variant.tmp");

  // 먼저 전체 enum을 빈 상태로 초기화
  builder.CreateStore(llvm::Constant::getNullValue(enumLayoutTy), enumAddr);

  // tag
  auto *tagPtr =
      builder.CreateStructGEP(enumLayoutTy, enumAddr, 0, "enum.tag.ptr");

  builder.CreateStore(builder.getInt32(expr->variant->ordinal), tagPtr);

  // payload slot: ptr
  auto *payloadPtr =
      builder.CreateStructGEP(enumLayoutTy, enumAddr, 1, "enum.payload.ptr");

  // unit variant
  if (expr->payload == nullptr) {
    builder.CreateStore(llvm::ConstantPointerNull::get(builder.getPtrTy()),
                        payloadPtr);

    auto *value = builder.CreateLoad(enumLayoutTy, enumAddr, "enum.variant");

    return {
        value,
        enumAddr,
        MIRValueCategory::OwnedTemp,
    };
  }

  auto *payloadType = expr->variant->payloadType;

  if (payloadType == nullptr) {
    Error::internal("enum variant payload expression has no payload type");
  }

  auto *payloadLlvmType = getType(payloadType);

  // 우변 payload 평가
  auto payloadValue = lowerValue(expr->payload.get(), ctx);

  /*
   * enum payload는 enum이 소유한다.
   *
   * 따라서 payloadValue.addr가 존재하더라도 그 주소를 enum에 직접 저장하면
   * 안 된다. 지역 변수나 임시 alloca의 주소일 수 있기 때문이다.
   *
   * 항상 별도의 heap 저장공간을 만들고 copy/move한다.
   */
  auto *ptrTy = builder.getPtrTy();
  auto *mallocTy =
      llvm::FunctionType::get(ptrTy, {builder.getInt64Ty()}, false);

  auto *payloadSize = llvm::ConstantExpr::getSizeOf(payloadLlvmType);

  auto *payloadAddr = builder.CreateCall(getRuntimeFunc("malloc", mallocTy),
                                         {payloadSize}, "enum.payload");

  // assign()은 string/struct의 기존 값을 먼저 destroy하므로 zero-init 필요
  builder.CreateStore(llvm::Constant::getNullValue(payloadLlvmType),
                      payloadAddr);

  /*
   * Borrowed/Plain이면 copy
   * OwnedTemp이면 move
   *
   * 구분은 assign() 내부에서 payloadValue.category를 보고 처리한다.
   */
  assign({payloadAddr, expr->payload->type}, payloadValue, payloadType, ctx);

  // enum이 새 heap payload를 소유
  builder.CreateStore(payloadAddr, payloadPtr);

  auto *value = builder.CreateLoad(enumLayoutTy, enumAddr, "enum.variant");

  return {
      value,
      enumAddr,
      MIRValueCategory::OwnedTemp,
  };
}

LoweredValue llvmCodegen::lowerRuntime(MIRRuntimeCallExpr *expr,
                                       FuncContext &ctx) {
  auto *runtime = getOrDeclareRuntimeFunction(expr->symbol);

  std::vector<llvm::Value *> args;
  vector<MIRValue *> values;
  for (auto &arg : expr->args) {
    values.push_back(arg.get());
  }
  auto out = lowerArgs(args, values, ctx);
  auto call = builder.CreateCall(runtime, args);
  for (auto &o : out) {
    builder.CreateCall(defaultDestroys.at(o.type), {o.addr});
  }

  return {call};
}

llvm::Function *llvmCodegen::getOrDeclareRuntimeFunction(RuntimeSymbol *rt) {
  if (auto it = runtimes.find(rt); it != runtimes.end()) {
    return it->second;
  }

  auto *ptrTy = llvm::PointerType::getUnqual(context);

  std::vector<llvm::Type *> paramTypes;
  for (auto *param : rt->params) {
    if (dynamic_cast<StringType *>(param)) {
      paramTypes.push_back(ptrTy);
    } else {
      paramTypes.push_back(getType(param));
    }
  }

  auto *retType = getType(rt->returnType);

  auto *funcTy = llvm::FunctionType::get(retType, paramTypes, false);

  auto *func = llvm::Function::Create(funcTy, llvm::Function::ExternalLinkage,
                                      rt->llvmName, llvmModule.get());

  runtimes.emplace(rt, func);
  return func;
}

LoweredValue llvmCodegen::lowerArrayInitExpr(MIRArrayInitExpr *expr,
                                             FuncContext &ctx) {
  auto *arrayTy = getType(expr->type);
  auto *arr = createEntryAlloca(ctx.func, arrayTy, "array.literal");

  auto *elementType = expr->elementType;

  // assign()이 destroy부터 수행하는 타입이 있으므로 초기 상태 보장
  if (needsDestroy(elementType)) {
    builder.CreateStore(llvm::Constant::getNullValue(arrayTy), arr);
  }

  for (size_t i = 0; i < expr->elements.size(); ++i) {
    auto raw = lowerValue(expr->elements[i].get(), ctx);

    // Resolver가 정한 공통 element type으로 맞춤
    auto value = castTo(raw, expr->elements[i]->type, elementType);

    auto *slot = builder.CreateGEP(
        arrayTy, arr,
        {builder.getInt32(0), builder.getInt32(static_cast<uint32_t>(i))});

    assign({slot, elementType}, value, elementType, ctx);
  }

  if (needsDestroy(expr->type)) {
    addClean(arr, expr->type, ctx);
  }

  return {
      builder.CreateLoad(arrayTy, arr, "array.literal.value"),
      arr,
      MIRValueCategory::OwnedTemp,
  };
}