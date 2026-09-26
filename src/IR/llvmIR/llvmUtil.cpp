#include "hrd/IR/MIR/MIRExpr.h"
#include "hrd/IR/MIR/MIRNode.h"
#include "hrd/IR/llvmIR/CodegenStructs.h"
#include "hrd/IR/llvmIR/llvmCodegen.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/enums/TypeKind.h"
#include "hrd/util/Error.h"
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <unordered_map>
#include <vector>

LoweredValue llvmCodegen::castTo(LoweredValue value, TypeSymbol *sourceType,
                                 TypeSymbol *targetType) {
  llvm::Type *src = value.value->getType();
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
      return {builder.CreateBitCast(value.value, dst, "ptrcasttmp"), value.addr,
              value.category};
    }
  }

  // string -> string
  if (isString(sourceType) && isString(targetType)) {
    const unsigned srcBits = dynamic_cast<StringType *>(sourceType)->bitWidth;
    const unsigned dstBits = dynamic_cast<StringType *>(targetType)->bitWidth;

    if (srcBits == dstBits) {
      return value;
    }

    auto *ptrTy = llvm::PointerType::getUnqual(context);
    auto *voidTy = builder.getVoidTy();

    auto *srcTy = getType(sourceType);
    auto *dstTy = getType(targetType);

    auto *srcPtr =
        builder.CreateAlloca(srcTy, nullptr, sourceType->name + ".cast.source");

    builder.CreateStore(value.value, srcPtr);

    auto *out =
        builder.CreateAlloca(dstTy, nullptr, targetType->name + ".cast.out");

    auto *fnTy = llvm::FunctionType::get(voidTy, {ptrTy, ptrTy}, false);

    builder.CreateCall(getRuntimeFunc("hrd_cast_" + sourceType->name + "_to_" +
                                          targetType->name,
                                      fnTy),
                       {out, srcPtr});

    auto *result =
        builder.CreateLoad(dstTy, out, targetType->name + ".cast.result");

    return {result, out, MIRValueCategory::OwnedTemp};
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
        return {builder.CreateZExt(value.value, dst, "zexttmp")};
      }
      return {builder.CreateSExt(value.value, dst, "sexttmp"), value.addr,
              value.category};
    }

    if (srcBits > dstBits) {
      return {builder.CreateTrunc(value.value, dst, "trunctmp"), value.addr,
              value.category};
    }

    return {value};
  }

  // float -> float
  if (src->isFloatingPointTy() && dst->isFloatingPointTy()) {
    uint64_t srcBits = src->getPrimitiveSizeInBits();
    uint64_t dstBits = dst->getPrimitiveSizeInBits();

    if (srcBits < dstBits) {
      return {builder.CreateFPExt(value.value, dst, "fpexttmp"), value.addr,
              value.category};
    }

    if (srcBits > dstBits) {
      return {builder.CreateFPTrunc(value.value, dst, "fptrunctmp"), value.addr,
              value.category};
    }

    return value;
  }

  // int -> float
  if (src->isIntegerTy() && dst->isFloatingPointTy()) {
    if (isUnsigned(sourceType)) {
      return {builder.CreateUIToFP(value.value, dst, "uitofptmp"), value.addr,
              value.category};
    }

    return {builder.CreateSIToFP(value.value, dst, "sitofptmp"), value.addr,
            value.category};
  }

  // float -> int
  if (src->isFloatingPointTy() && dst->isIntegerTy()) {
    if (isUnsigned(targetType)) {
      return {builder.CreateFPToUI(value.value, dst, "fptouitmp"), value.addr,
              value.category};
    }

    return {builder.CreateFPToSI(value.value, dst, "fptositmp"), value.addr,
            value.category};
  }

  Error::internal("invalid cast");
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
  if (t == nullptr) {
    Error::internal("typeSymbol is nullptr");
  }
  if (auto g = dynamic_cast<GenericSymbol *>(t)) {
    if (dynamic_cast<HandleSymbol *>(g->origin)) {
      return getHrdHandleType();
    }
    return getOrCreateGeneric(g);
  }

  if (t->kind == TypeKind::CLASS) {
    return llvm::PointerType::getUnqual(context);
  }

  return getLayoutType(t);
}

llvm::Type *llvmCodegen::getLayoutType(TypeSymbol *t) {
  if (auto *generic = dynamic_cast<GenericSymbol *>(t)) {
    if (dynamic_cast<HandleSymbol *>(generic->origin)) {
      return getHrdHandleType();
    }
    return getOrCreateGeneric(generic);
  }

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
  if (type->kind == TypeKind::CLASS) {
    Error::internal("class observer cannot be stored as field");
  }

  return getType(type);
}

bool llvmCodegen::needsDestroy(TypeSymbol *type) {

  if (auto arr = dynamic_cast<ArrayTypeSymbol *>(type)) {
    return needsDestroy(arr->baseType);
  }

  return isString(type) || type->kind == TypeKind::STRUCT ||
         type->kind == TypeKind::ENUM;
}

llvm::Type *llvmCodegen::getHrdHandleType() {
  return llvm::Type::getInt64Ty(context);
}

std::string llvmCodegen::getStringSuffix(TypeSymbol *type) {
  if (type == table.registry.getBuilt("s8")) {
    return "s8";
  }

  if (type == table.registry.getBuilt("s16")) {
    return "s16";
  }

  if (type == table.registry.getBuilt("s32")) {
    return "s32";
  }

  Error::internal("invalid string type");
}

llvm::Function *llvmCodegen::getOrDeclareFunction(MethodSymbol *method) {
  auto it = funcs.find(method);
  if (it != funcs.end()) {
    return it->second;
  }

  auto rt = getType(method->returnType);
  if (rt == nullptr) {
    Error::internal("null llvm return type: " + method->name);
  }

  vector<llvm::Type *> params;
  params.push_back(llvm::PointerType::get(context, 0));
  for (auto &p : method->params) {
    auto pt = getType(p->typeSymbol);
    if (pt == nullptr) {
      Error::internal("null llvm param type: " + p->name);
    }
    params.push_back(pt);
  }
  auto fnType = llvm::FunctionType::get(rt, params, false);

  auto *fn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage,
                                    mangle(method), llvmModule.get());

  funcs.emplace(method, fn);
  return fn;
}

bool llvmCodegen::needSelf(MIRFunction *func) {
  auto method = func->symbol;
  return needSelf(method);
}

bool llvmCodegen::needSelf(MethodSymbol *m) {
  return !(m->isExtern || m->isStatic);
}

llvm::Function *llvmCodegen::getOrCreateFunc(MethodSymbol *symbol,
                                             MethodContext &ctx) {
  if (ctx.isGenericDecl) {
    if (ctx.key == nullptr || ctx.substitution == nullptr) {
      Error::internal("illegal generic method");
    }

    auto it = genericMethodMap.find(*ctx.key);
    if (it != genericMethodMap.end()) {
      return it->second;
    }

    llvm::IRBuilderBase::InsertPointGuard insertPointGuard(builder);

    auto resolved = resolveGenericType(symbol->returnType, *ctx.substitution);
    auto rt = getType(resolved);

    if (rt == nullptr) {
      Error::internal("null llvm return type: " + symbol->name);
    }

    vector<llvm::Type *> params;

    bool hasSelf = needSelf(symbol);

    if (hasSelf) {
      params.push_back(llvm::PointerType::get(context, 0));
    }

    for (auto &p : symbol->params) {
      auto t = resolveGenericType(p->typeSymbol, *ctx.substitution);
      auto pt = getType(t);

      if (pt == nullptr) {
        Error::internal("null llvm param type: " + p->name);
      }

      params.push_back(pt);
    }

    auto fnType = llvm::FunctionType::get(rt, params, false);

    auto fn =
        llvm::Function::Create(fnType, llvm::GlobalValue::LinkOnceODRLinkage,
                               mangle(symbol, *ctx.key), llvmModule.get());

    // Generic method용 prologue block.
    // MIR block보다 먼저 만들어야 function entry block이 된다.
    auto *genericEntry = llvm::BasicBlock::Create(context, "genericentry", fn);

    unordered_map<BlockID, llvm::BasicBlock *> blocks;

    MIRFunction *m = nullptr;

    {
      auto &methodMap = program->genericMap[symbol->owner].methodMap;
      auto i = methodMap.find(symbol);

      if (i == methodMap.end()) {
        Error::internal("fail to get origin MIRFunction");
      }

      m = i->second;
    }

    for (auto &block : m->blocks) {
      blocks.emplace(block->id,
                     llvm::BasicBlock::Create(
                         context, "bb" + std::to_string(block->id), fn));
    }

    localMap locals;

    ParamMap paramMap;

    std::vector<Cleanup> cleanupStack;

    std::unordered_set<llvm::Value *> canceledCleanups;

    auto argIt = fn->arg_begin();

    llvm::Argument *self = nullptr;

    if (hasSelf) {
      self = &*argIt++;
      self->setName("self");
      paramMap.emplace(symbol->selfReceiver, self);
    }

    auto selfType = resolveGenericType(symbol->owner, *ctx.substitution);

    FuncContext fctx = {
        fn,       blocks,       locals,           paramMap,        self,
        selfType, cleanupStack, canceledCleanups, ctx.substitution};

    // 아래 parameter store와 entry branch는 반드시 새 generic method 안에
    // 생성되어야 한다.
    builder.SetInsertPoint(genericEntry);

    for (auto *param : symbol->params) {
      llvm::Argument *arg = &*argIt++;

      arg->setName(param->name);

      auto *paramType =
          getType(resolveGenericType(param->typeSymbol, *ctx.substitution));

      if (paramType == nullptr) {
        Error::internal("null llvm param type: " + param->name);
      }

      auto *slot = createEntryAlloca(fn, paramType, param->name);

      builder.CreateStore(arg, slot);

      fctx.params.emplace(param, slot);
    }

    builder.CreateBr(blocks.at(m->entry));

    for (auto &block : m->blocks) {
      lowerBlock(block.get(), fctx);
    }

    genericMethodMap.emplace(*ctx.key, fn);

    return fn;
  }

  auto it = funcs.find(symbol);

  if (it != funcs.end()) {
    return it->second;
  }

  auto fn = createFuncShell(symbol);

  funcs.emplace(symbol, fn);

  return fn;
}

llvm::Function *llvmCodegen::createFuncShell(MethodSymbol *symbol) {
  auto rt = getType(symbol->returnType);
  if (rt == nullptr) {
    Error::internal("null llvm return type: " + symbol->name);
  }

  vector<llvm::Type *> params;
  if (needSelf(symbol)) {
    params.push_back(llvm::PointerType::get(context, 0));
  }
  for (auto &p : symbol->params) {
    auto pt = getType(p->typeSymbol);
    if (pt == nullptr) {
      Error::internal("null llvm param type: " + p->name);
    }
    params.push_back(pt);
  }
  auto fnType = llvm::FunctionType::get(rt, params, false);
  auto fn = llvm::Function::Create(fnType, llvm::GlobalValue::ExternalLinkage,
                                   mangle(symbol), llvmModule.get());
  return fn;
}

bool llvmCodegen::isClass(TypeSymbol *type) {
  if (auto gen = dyn_cast<GenericSymbol>(type)) {
    return gen->origin->kind == TypeKind::CLASS;
  }
  return type->kind == TypeKind::CLASS;
}