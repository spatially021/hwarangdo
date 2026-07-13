#include "hrd/IR/MIR/MIRNode.h"
#include "hrd/IR/llvmIR/llvmCodegen.h"
#include "hrd/SemanticAnalyzer/ResolvedLit.h"
#include "hrd/util/Error.h"
#include <llvm/IR/Constants.h>
void llvmCodegen::lowerStringSwitch(const SwitchTerminator &t,
                                    FuncContext &ctx) {
  auto cond = lowerValue(t.cond.get(), ctx);

  if (!cond.value) {
    Error::internal("string switch condition has no value");
  }

  auto *checkBB = builder.GetInsertBlock();
  auto *parentFunc = checkBB->getParent();

  auto *type = t.cond->type;
  auto *strTy = getLayoutType(type); // %string8 = { ptr, i64, i64 }
  auto *ptrTy = llvm::PointerType::getUnqual(context);

  auto *callTy =
      llvm::FunctionType::get(builder.getInt1Ty(), {ptrTy, ptrTy}, false);

  auto *eqFunc = getRuntimeFunc("hrd_string_eq_s8", callTy);
  if (!eqFunc) {
    Error::internal("missing runtime function: hrd_string_eq_s8");
  }

  auto *defaultBB = ctx.blocks.at(t.defaultTarget);

  if (t.cases.empty()) {
    builder.CreateBr(defaultBB);
    return;
  }

  // switch 기준값은 모든 case에서 동일하므로 한 번만 materialize한다.
  auto *condAddr = materializeAddress(cond, strTy, "switch.cond.str.addr");
  builder.CreateStore(cond.value, condAddr);

  for (size_t i = 0; i < t.cases.size(); ++i) {
    const auto &c = t.cases[i];

    auto *literal = std::get_if<ResolvedLit>(&c.value);
    if (!literal || !literal->isString()) {
      Error::internal("non-string case in string switch");
    }

    builder.SetInsertPoint(checkBB);

    auto caseValue = lowerStringLiteral(literal->asString(), type);

    if (!caseValue.value) {
      Error::internal("string case literal has no value");
    }

    auto *caseAddr =
        materializeAddress(caseValue, strTy, "switch.case.str.addr");
    builder.CreateStore(caseValue.value, caseAddr);

    auto *matched =
        builder.CreateCall(eqFunc, {condAddr, caseAddr}, "switch.s8.eq");

    // lowerStringLiteral이 owned allocation을 만든다면 비교 직후 파괴해야 한다.
    if (caseValue.category == MIRValueCategory::OwnedTemp &&
        caseValue.addr != nullptr) {
      if (needsDestroy(type)) {
        auto it = defaultDestroys.find(type);
        if (it == defaultDestroys.end()) {
          Error::internal("fail to get destroy : " + type->name);
        }
        builder.CreateCall(it->second, {caseValue.addr});
      }
    }

    auto *successBB = ctx.blocks.at(c.target);

    llvm::BasicBlock *failBB;

    if (i + 1 == t.cases.size()) {
      failBB = defaultBB;
    } else {
      failBB =
          llvm::BasicBlock::Create(context, "switch.string.check", parentFunc);
    }

    builder.CreateCondBr(matched, successBB, failBB);
    checkBB = failBB;
  }
}

void llvmCodegen::lowerNativeSwitch(const SwitchTerminator &t,
                                    FuncContext &ctx) {
  auto cond = lowerValue(t.cond.get(), ctx);

  auto *condType = llvm::dyn_cast<llvm::IntegerType>(cond.value->getType());

  if (!condType) {
    Error::internal("native switch condition is not integer");
  }

  auto *defaultBB = ctx.blocks.at(t.defaultTarget);

  auto *sw = builder.CreateSwitch(cond.value, defaultBB,
                                  static_cast<unsigned>(t.cases.size()));

  for (const auto &c : t.cases) {
    auto *lit = std::get_if<ResolvedLit>(&c.value);

    if (!lit) {
      Error::internal("non-literal case in native switch");
    }

    llvm::ConstantInt *caseValue = nullptr;

    if (lit->isBool()) {
      caseValue = llvm::ConstantInt::getBool(context, lit->asBool());
    }

    if (lit->isInt()) {
      caseValue = static_cast<llvm::ConstantInt *>(
          llvm::ConstantInt::get(condType, lit->asInt().value));
    }

    if (lit->isChar()) {
      caseValue = llvm::ConstantInt::get(condType, lit->asChar().codePoint);
    }

    if (lit->isFloat()) {
      Error::internal("float literal cannot be used in native switch");
    }

    if (lit->isString()) {
      Error::internal("string literal cannot be used in native switch");
    }

    if (caseValue == nullptr) {
      Error::internal("unsupported switch literal");
    }

    sw->addCase(caseValue, ctx.blocks.at(c.target));
  }
}

void llvmCodegen::lowerEnumSwitch(const SwitchTerminator &t, FuncContext &ctx) {
  auto cond = lowerValue(t.cond.get(), ctx);
  auto *tag = extractEnumTag(cond, t.cond->type);

  if (t.cases.size() > std::numeric_limits<unsigned>::max()) {
    Error::internal("too many enum switch cases");
  }

  auto *sw = builder.CreateSwitch(tag, ctx.blocks.at(t.defaultTarget),
                                  static_cast<unsigned>(t.cases.size()));

  for (const auto &c : t.cases) {
    auto *variantSlot = std::get_if<EnumVariantSymbol *>(&c.value);

    if (variantSlot == nullptr || *variantSlot == nullptr) {
      Error::internal("non-enum case in enum switch");
    }

    sw->addCase(builder.getInt32((*variantSlot)->ordinal),
                ctx.blocks.at(c.target));
  }
}