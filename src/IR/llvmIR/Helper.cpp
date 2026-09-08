#include "hrd/IR/MIR/MIRExpr.h"
#include "hrd/IR/llvmIR/llvmCodegen.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include <llvm/IR/Value.h>

vector<Cleanup> llvmCodegen::lowerArgs(vector<llvm::Value *> &args,
                                       vector<MIRValue *> values,
                                       FuncContext &ctx) {
  vector<Cleanup> out;

  for (auto *v : values) {
    auto lv = lowerValue(v, ctx);

    if (isString(v->type)) {
      if (lv.addr == nullptr) {
        Error::internal("string argument has no address");
      }

      // string runtime/function ABI는 string* 전달
      args.push_back(lv.addr);
    } else {
      args.push_back(lv.value);
    }

    if (lv.category == MIRValueCategory::OwnedTemp && lv.addr != nullptr &&
        needsDestroy(v->type)) {
      out.push_back({lv.addr, v->type});
    }
  }

  return out;
}

llvm::Value *llvmCodegen::lowerReceiverPtr(MIRPlace *place, FuncContext &ctx) {
  auto *addr = lowerPlace(place, ctx).dst;
  auto *ty = place->symbol->typeSymbol;

  if (ty->kind == TypeKind::CLASS && dynamic_cast<MIRLocalPlace *>(place)) {
    return builder.CreateLoad(builder.getPtrTy(), addr, "receiver.ptr");
  }

  return addr;
}

llvm::Value *llvmCodegen::materializeAddress(const LoweredValue &value,
                                             llvm::Type *layoutType,
                                             llvm::StringRef name) {
  if (value.addr) {
    return value.addr;
  }

  if (!value.value) {
    Error::internal("cannot materialize value without LLVM value");
  }

  auto *addr = builder.CreateAlloca(layoutType, nullptr, name);
  builder.CreateStore(value.value, addr);
  return addr;
}
llvm::Value *llvmCodegen::extractEnumTag(const LoweredValue &value,
                                         TypeSymbol *enumType) {
  auto *layoutTy = getLayoutType(enumType);

  if (value.addr != nullptr) {
    auto *tagPtr =
        builder.CreateStructGEP(layoutTy, value.addr, 0, "enum.tag.ptr");

    return builder.CreateLoad(builder.getInt32Ty(), tagPtr, "enum.tag");
  }

  if (value.value != nullptr) {
    auto *tag = builder.CreateExtractValue(value.value, {0}, "enum.tag");

    if (!tag->getType()->isIntegerTy(32)) {
      Error::internal("enum extracted tag must be i32");
    }

    return tag;
  }

  Error::internal("enum value has neither value nor address");
}
