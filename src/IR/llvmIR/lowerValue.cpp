#include "hrd/IR/MIR/MIRExpr.h"
#include "hrd/IR/llvmIR/llvmCodegen.h"
#include "hrd/enums/Operator.h"
#include "hrd/util/Error.h"
#include <llvm/IR/Value.h>
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

llvm::Value *llvmCodegen::lowerValue(MIRValue *value, FuncContext &ctx) {
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

llvm::Value *llvmCodegen::lowerBinaryExpr(MIRBinaryExpr *expr,
                                          FuncContext &ctx) {
  if (expr->op == Operator::AND) {
    return lowerLogicalAnd(expr, ctx);
  }

  if (expr->op == Operator::OR) {
    return lowerLogicalOr(expr, ctx);
  }

  auto type = isCompare(expr->op) ? expr->operandType : expr->type;

  auto lhs = castTo(lowerValue(expr->lhs.get(), ctx), expr->lhs->type, type);

  auto rawRhs = lowerValue(expr->rhs.get(), ctx);

  bool usePowi = expr->op == Operator::POW && isInt(expr->rhs->type) &&
                 isFloat(expr->type);

  auto rhs = usePowi ? castTo(rawRhs, expr->rhs->type, table->getBuilt("i32"))
                     : castTo(rawRhs, expr->rhs->type, type);
  switch (expr->op) {

  case Operator::ADD:
    return isFloat(type) ? builder.CreateFAdd(lhs, rhs)
                         : builder.CreateAdd(lhs, rhs);

  case Operator::SUB:
    return isFloat(type) ? builder.CreateFSub(lhs, rhs)
                         : builder.CreateSub(lhs, rhs);

  case Operator::MUL:
    return isFloat(type) ? builder.CreateFMul(lhs, rhs)
                         : builder.CreateMul(lhs, rhs);

  case Operator::DIV: {
    if (isFloat(type)) {
      return builder.CreateFDiv(lhs, rhs);
    }
    if (isUnsigned(type)) {
      return builder.CreateUDiv(lhs, rhs);
    }
    return builder.CreateSDiv(lhs, rhs);
  }
  case Operator::REM: {
    if (isFloat(type)) {
      return builder.CreateFRem(lhs, rhs);
    }
    if (isUnsigned(type)) {
      return builder.CreateURem(lhs, rhs);
    }
    return builder.CreateSRem(lhs, rhs);
  }
  case Operator::POW: {
    if (isInt(expr->lhs->type) && isInt(expr->rhs->type)) {
      Error::internal("not developed int ** int");
    }

    if (usePowi) {
      auto pow = llvm::Intrinsic::getOrInsertDeclaration(
          llvmModule.get(), llvm::Intrinsic::powi, {getType(expr->type)});

      return builder.CreateCall(pow, {lhs, rhs});
    }

    auto pow = llvm::Intrinsic::getOrInsertDeclaration(
        llvmModule.get(), llvm::Intrinsic::pow, {getType(expr->type)});

    return builder.CreateCall(pow, {lhs, rhs});
  }
  case Operator::B_AND: {
    return builder.CreateAnd(lhs, rhs);
  }
  case Operator::B_OR:
    return builder.CreateOr(lhs, rhs);
  case Operator::B_XOR:
    return builder.CreateXor(lhs, rhs);

  case Operator::EQ:

  case Operator::NT:
  case Operator::LS:
  case Operator::LSE:
  case Operator::GR:
  case Operator::GRE:
    // TODO: world처리 후 정책 정해서 처리하기.
    Error::internal("compare operator not developed");

  case Operator::LSH: {
    llvm::Value *amount = castTo(rhs, expr->rhs->type, expr->lhs->type);
    return builder.CreateShl(lhs, amount);
  }

  case Operator::RSH: {
    llvm::Value *amount = castTo(rhs, expr->rhs->type, expr->lhs->type);

    return isUnsigned(expr->lhs->type) ? builder.CreateLShr(lhs, amount)
                                       : builder.CreateAShr(lhs, amount);
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
  Error::internal("in binary but unary detected");
}

llvm::Value *llvmCodegen::lowerLogicalAnd(MIRBinaryExpr *expr,
                                          FuncContext &ctx) {
  llvm::Function *func = builder.GetInsertBlock()->getParent();

  llvm::BasicBlock *rhsBlock =
      llvm::BasicBlock::Create(context, "land.rhs", func);
  llvm::BasicBlock *falseBlock =
      llvm::BasicBlock::Create(context, "land.false", func);
  llvm::BasicBlock *mergeBlock =
      llvm::BasicBlock::Create(context, "land.merge", func);

  llvm::Value *lhs = lowerValue(expr->lhs.get(), ctx);
  builder.CreateCondBr(lhs, rhsBlock, falseBlock);

  builder.SetInsertPoint(rhsBlock);
  llvm::Value *rhs = lowerValue(expr->rhs.get(), ctx);
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

  return phi;
}

llvm::Value *llvmCodegen::lowerLogicalOr(MIRBinaryExpr *expr,
                                         FuncContext &ctx) {
  llvm::Function *func = builder.GetInsertBlock()->getParent();

  llvm::BasicBlock *trueBlock =
      llvm::BasicBlock::Create(context, "lor.true", func);
  llvm::BasicBlock *rhsBlock =
      llvm::BasicBlock::Create(context, "lor.rhs", func);
  llvm::BasicBlock *mergeBlock =
      llvm::BasicBlock::Create(context, "lor.merge", func);

  llvm::Value *lhs = lowerValue(expr->lhs.get(), ctx);
  builder.CreateCondBr(lhs, trueBlock, rhsBlock);

  builder.SetInsertPoint(trueBlock);
  llvm::Value *trueVal = llvm::ConstantInt::getTrue(context);
  builder.CreateBr(mergeBlock);
  trueBlock = builder.GetInsertBlock();

  builder.SetInsertPoint(rhsBlock);
  llvm::Value *rhs = lowerValue(expr->rhs.get(), ctx);
  builder.CreateBr(mergeBlock);
  rhsBlock = builder.GetInsertBlock();

  builder.SetInsertPoint(mergeBlock);
  llvm::PHINode *phi =
      builder.CreatePHI(llvm::Type::getInt1Ty(context), 2, "lor.result");

  phi->addIncoming(trueVal, trueBlock);
  phi->addIncoming(rhs, rhsBlock);

  return phi;
}

llvm::Value *llvmCodegen::lowerLoad(MIRLoad *expr, FuncContext &ctx) {
  auto ptr = lowerPlace(expr->place.get(), ctx);
  llvm::Type *valueTy = getType(expr->type);
  return builder.CreateLoad(valueTy, ptr, "loadtmp");
}

llvm::Value *llvmCodegen::lowerPayloadExtractExpr(MIRPayloadExtractExpr *expr,
                                                  FuncContext &ctx) {
  // TODO: enum 구현 후 처리
}

llvm::Value *llvmCodegen::lowerLiteralExpr(MIRLiteralExpr *expr,
                                           FuncContext &) {
  auto lit = expr->literal;
  llvm::Type *ty = getType(lit.type);

  if (lit.isBool()) {
    return llvm::ConstantInt::getBool(context, lit.asBool());
  }

  if (lit.isInt()) {
    return llvm::ConstantInt::get(ty, lit.asInt().value);
  }

  if (lit.isFloat()) {
    return llvm::ConstantFP::get(context, lit.asFloat().value);
  }

  if (lit.isChar()) {
    return llvm::ConstantInt::get(ty, lit.asChar().codePoint);
  }

  if (lit.isString()) {
    return lowerStringLiteral(lit.asString(), lit.type);
  }

  Error::internal("unknwon literal type");
}

llvm::Value *llvmCodegen::lowerStringLiteral(const StringPayload &payload,
                                             TypeSymbol *type) {
  auto *stringTy = getType(type); // %string8 = { ptr, i64 }
  auto *i64Ty = llvm::Type::getInt64Ty(context);

  std::string bytes;
  bytes.reserve(payload.codePoints.size() + 1);

  for (uint32_t cp : payload.codePoints) {
    // 지금은 s8만 우선.
    if (cp > 0x7F) {
      Error::internal("non-ascii string literal in s8 lowering");
    }
    bytes.push_back(static_cast<char>(cp));
  }

  auto *dataPtr = builder.CreateGlobalStringPtr(bytes);

  llvm::Value *result = llvm::PoisonValue::get(stringTy);

  result = builder.CreateInsertValue(result, dataPtr, {0});
  result = builder.CreateInsertValue(
      result, llvm::ConstantInt::get(i64Ty, payload.codePoints.size()), {1});

  return result;
}

llvm::Value *llvmCodegen::lowerUnaryExpr(MIRUnaryExpr *expr, FuncContext &ctx) {
  auto operand = lowerValue(expr->operrand.get(), ctx);
  switch (expr->op) {

  case Operator::L_NOT:
  case Operator::B_NOT:
    return builder.CreateNot(operand);

  case Operator::PLUS:
    return operand;
  case Operator::MINUS:
    return isFloat(expr->type) ? builder.CreateFNeg(operand)
                               : builder.CreateNeg(operand);

  default:
    Error::internal("expect unary but use binary");
  }
}

llvm::Value *llvmCodegen::lowerCastExpr(MIRCastExpr *expr, FuncContext &ctx) {
  auto operand = lowerValue(expr->operrand.get(), ctx);
  return castTo(operand, expr->from, expr->to);
}

llvm::Value *llvmCodegen::lowerCallExpr(MIRCallExpr *expr, FuncContext &ctx) {
  auto callee = funcs.at(expr->method);

  std::vector<llvm::Value *> args;

  // self
  if (auto l = dynamic_cast<MIRLoad *>(expr->base.get())) {
    args.push_back(lowerPlace(l->place.get(), ctx));
  } else {
    Error::internal("fail to get receiver");
  }

  // 일반 인자
  for (auto &arg : expr->args) {
    args.push_back(lowerValue(arg.get(), ctx));
  }

  return builder.CreateCall(callee, args);
}

llvm::Value *llvmCodegen::lowerStructInitExpr(MIRStructInitExpr *expr,
                                              FuncContext &ctx) {
  auto *structTy = getType(expr->type); // %Vec2 같은 struct type

  // 1. 임시 struct 공간 생성
  llvm::AllocaInst *tmp =
      createEntryAlloca(ctx.func, structTy, "struct.init.tmp");

  // 2. 인자 준비: self ptr + 일반 args
  std::vector<llvm::Value *> args;
  args.push_back(tmp);

  for (auto &arg : expr->args) {
    args.push_back(lowerValue(arg.get(), ctx));
  }

  // 3. init 호출
  if (expr->initMethod != nullptr) {
    auto *initFn = funcs.at(expr->initMethod);
    builder.CreateCall(initFn, args);
  } else {
    // 기본 init: init이 정의되지 않은 T()만 허용된 상태라면
    // 필드 기본값 정책이 없으면 아무것도 안 해도 됨.
    // 단, 미초기화 읽기 검증은 앞단에서 잡는다는 전제.
  }

  // 4. expression value로 반환
  return builder.CreateLoad(structTy, tmp, "struct.init.val");
}

void llvmCodegen::lowerStructInitTo(MIRStructInitExpr *expr, llvm::Value *dst,
                                    FuncContext &ctx) {
  std::vector<llvm::Value *> args;
  args.push_back(dst);

  for (auto &arg : expr->args) {
    args.push_back(lowerValue(arg.get(), ctx));
  }

  if (expr->initMethod != nullptr) {
    builder.CreateCall(funcs.at(expr->initMethod), args);
  }
}

llvm::Value *llvmCodegen::lowerVariantExpr(MIRVariantExpr *expr,
                                           FuncContext &ctx) {
  // TODO: enum 구현후 작성
}

llvm::Value *llvmCodegen::lowerViewExpr(MIRViewExpr *expr, FuncContext &ctx) {}

llvm::Value *llvmCodegen::lowerSpawnExpr(MIRSpawnExpr *expr, FuncContext &ctx) {

}

llvm::Value *llvmCodegen::lowerRuntime(MIRRuntimeCallExpr *expr,
                                       FuncContext &ctx) {
  auto runtime = getOrDeclareRuntimeFunction(expr->symbol);
  vector<llvm::Value *> args;
  for (auto &a : expr->args) {
    args.push_back(lowerValue(a.get(), ctx));
  }
  return builder.CreateCall(runtime, args);
}

llvm::Function *llvmCodegen::getOrDeclareRuntimeFunction(RuntimeSymbol *rt) {
  if (auto it = runtimes.find(rt); it != runtimes.end()) {
    return it->second;
  }

  std::vector<llvm::Type *> paramTypes;
  for (auto *param : rt->params) {
    paramTypes.push_back(getType(param));
  }

  auto *retType = getType(rt->returnType);

  auto *funcTy = llvm::FunctionType::get(retType, paramTypes, false);

  auto *func = llvm::Function::Create(funcTy, llvm::Function::ExternalLinkage,
                                      rt->llvmName, llvmModule.get());

  runtimes.emplace(rt, func);
  return func;
}