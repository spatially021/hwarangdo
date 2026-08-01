#include "hrd/IR/llvmIR/llvmCodegen.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/util/Error.h"
#include <iostream>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>

void llvmCodegen::buildTypes() {
  for (auto &t : table.types) {

    if (auto p = dynamic_cast<PrimtiveType *>(t)) {
      types.emplace(t, buildPrimitiveType(p));
      continue;
    }

    switch (t->kind) {

    case TypeSymbol::TypeKind::VOID: {
      types.emplace(t, llvm::Type::getVoidTy(context));
      continue;
    }
    case TypeSymbol::TypeKind::ARRAY:

    case TypeSymbol::TypeKind::HANDLE:

    case TypeSymbol::TypeKind::FUNC:

    case TypeSymbol::TypeKind::RESULT:
    case TypeSymbol::TypeKind::OPTION:
    case TypeSymbol::TypeKind::ERROR:
    case TypeSymbol::TypeKind::BUILTIN:

    default:
      break;
    }

    if (t->kind == TypeSymbol::TypeKind::CLASS ||
        t->kind == TypeSymbol::TypeKind::STRUCT) {
      auto type = llvm::StructType::create(context, t->name);
      types.emplace(t, type);
      auto *fnTy = llvm::FunctionType::get(builder.getVoidTy(),
                                           {builder.getPtrTy()}, false);

      llvm::Function *fn =
          llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage,
                                 t->name + "destroy.field", llvmModule.get());

      auto [it, inserted] = defaultDestroys.emplace(t, fn);

      if (!inserted) {
        std::cerr << "duplicate destroy: " << t->name << "\n";
      }
      continue;
    }

    if (t->kind == TypeSymbol::TypeKind::ENUM) {
      auto *st = llvm::StructType::create(context, t->name);
      enums.emplace(t, st);

      std::vector<llvm::Type *> fields;

      fields.push_back(builder.getInt32Ty()); // tag
      fields.push_back(builder.getPtrTy());   // payload

      st->setBody(fields);
      auto *fnTy = llvm::FunctionType::get(builder.getVoidTy(),
                                           {builder.getPtrTy()}, false);

      llvm::Function *fn =
          llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage,
                                 t->name + "destroy.field", llvmModule.get());

      defaultDestroys.emplace(t, fn);
    }
  }

  for (auto &t : table.types) {
    if (auto arr = dynamic_cast<ArrayTypeSymbol *>(t)) {
      buildArrayType(arr);
    }
  }

  for (auto &t : table.types) {
    if (auto arr = dynamic_cast<ArrayTypeSymbol *>(t)) {
      declareArrayDestroy(arr);
    }
  }

  for (auto &t : table.types) {

    if (dynamic_cast<PrimtiveType *>(t)) {
      continue;
    }

    if (t->kind == TypeSymbol::TypeKind::STRUCT ||
        t->kind == TypeSymbol::TypeKind::CLASS) {
      auto *type = llvm::dyn_cast<llvm::StructType>(getLayoutType(t));
      if (type == nullptr) {
        Error::internal("illegal llvm type");
      }

      vector<llvm::Type *> fields;
      for (auto &f : t->fields) {
        fields.push_back(getFieldType(f->typeSymbol));
      }

      type->setBody(fields);
      emitDefaultDestroy(t);
      continue;
    }

    if (t->kind == TypeSymbol::TypeKind::ENUM) {
      auto *fn = defaultDestroys.at(t);
      auto *layoutTy = getLayoutType(t);

      auto *entry = llvm::BasicBlock::Create(context, "entry", fn);
      auto *doneBB = llvm::BasicBlock::Create(context, "done", fn);

      builder.SetInsertPoint(entry);

      auto *self = fn->getArg(0);

      auto *tagPtr = builder.CreateStructGEP(layoutTy, self, 0);
      auto *tag = builder.CreateLoad(builder.getInt32Ty(), tagPtr);

      auto *payloadPtr = builder.CreateStructGEP(layoutTy, self, 1);
      auto *payload = builder.CreateLoad(builder.getPtrTy(), payloadPtr);

      auto *sw = builder.CreateSwitch(
          tag, doneBB, static_cast<unsigned int>(t->variants.size()));

      auto *ptrTy = builder.getPtrTy();
      auto *voidTy = builder.getVoidTy();

      auto *freeTy = llvm::FunctionType::get(voidTy, {ptrTy}, false);
      auto *freeFn = getRuntimeFunc("free", freeTy);

      for (auto &v : t->variants) {
        auto *variant = v.get();

        // unit variant는 heap payload가 없음
        if (variant->payloadType == nullptr) {
          continue;
        }

        auto *caseBB =
            llvm::BasicBlock::Create(context, "destroy." + variant->name, fn);

        auto *freeBB =
            llvm::BasicBlock::Create(context, "free." + variant->name, fn);

        sw->addCase(builder.getInt32(variant->ordinal), caseBB);

        builder.SetInsertPoint(caseBB);

        // zero-init된 enum이나 이미 move된 enum도 안전하게 처리
        auto *isNull = builder.CreateICmpEQ(
            payload, llvm::ConstantPointerNull::get(
                         llvm::cast<llvm::PointerType>(ptrTy)));

        builder.CreateCondBr(isNull, doneBB, freeBB);

        builder.SetInsertPoint(freeBB);

        // string, struct 등 내부 리소스가 있는 타입만 destroy
        if (needsDestroy(variant->payloadType)) {
          auto *destroyFn = defaultDestroys.at(variant->payloadType);
          builder.CreateCall(destroyFn, {payload});
        }

        // payload 저장공간 자체는 타입과 무관하게 항상 해제
        builder.CreateCall(freeFn, {payload});

        builder.CreateBr(doneBB);
      }

      builder.SetInsertPoint(doneBB);

      // destroy 이후 재파괴해도 안전하도록 enum 비우기
      auto *empty = llvm::ConstantAggregateZero::get(
          llvm::cast<llvm::StructType>(layoutTy));

      builder.CreateStore(empty, self);

      builder.CreateRetVoid();

      continue;
    }
  }

  for (auto &t : table.types) {
    if (auto arr = dynamic_cast<ArrayTypeSymbol *>(t)) {
      emitArrayDestroy(arr);
    }
  }
}

llvm::Type *llvmCodegen::buildPrimitiveType(PrimtiveType *type) {
  if (type == nullptr) {
    Error::internal("falled to cast typeSymbol to primitiveType");
  }

  switch (type->builtinType) {

  case BuiltInType::I8:
    return llvm::Type::getInt8Ty(context);
  case BuiltInType::I16:
    return llvm::Type::getInt16Ty(context);
  case BuiltInType::I32:
    return llvm::Type::getInt32Ty(context);
  case BuiltInType::I64:
    return llvm::Type::getInt64Ty(context);
  case BuiltInType::I128:
    return llvm::Type::getInt128Ty(context);
  case BuiltInType::U8:
    return llvm::Type::getInt8Ty(context);
  case BuiltInType::U16:
    return llvm::Type::getInt16Ty(context);
  case BuiltInType::U32:
    return llvm::Type::getInt32Ty(context);
  case BuiltInType::U64:
    return llvm::Type::getInt64Ty(context);
  case BuiltInType::U128:
    return llvm::Type::getInt128Ty(context);
  case BuiltInType::F16:
    return llvm::Type::getHalfTy(context);
  case BuiltInType::F32:
    return llvm::Type::getFloatTy(context);
  case BuiltInType::F64:
    return llvm::Type::getDoubleTy(context);
  case BuiltInType::F128:
    return llvm::Type::getFP128Ty(context);
  case BuiltInType::C8:
    return llvm::Type::getInt8Ty(context);
  case BuiltInType::C16:
    return llvm::Type::getInt16Ty(context);
  case BuiltInType::C32:
    return llvm::Type::getInt32Ty(context);
  case BuiltInType::S8: {
    auto s = llvm::StructType::create(context, "string8");
    s->setBody({
        llvm::PointerType::get(context, 0), // data
        llvm::Type::getInt64Ty(context),    // len
        llvm::Type::getInt64Ty(context),
    });
    auto *fn = getRuntimeFunc(
        "hrd_destroy_s8", llvm::FunctionType::get(builder.getVoidTy(),
                                                  {builder.getPtrTy()}, false));
    defaultDestroys.emplace(type, fn);
    return s;
  }

  case BuiltInType::S16: {
    auto s = llvm::StructType::create(context, "string16");
    s->setBody({
        llvm::PointerType::get(context, 0), // data
        llvm::Type::getInt64Ty(context),    // len
        llvm::Type::getInt64Ty(context),
    });
    auto *fn =
        getRuntimeFunc("hrd_destroy_s16",
                       llvm::FunctionType::get(builder.getVoidTy(),
                                               {builder.getPtrTy()}, false));
    defaultDestroys.emplace(type, fn);
    return s;
  }
  case BuiltInType::S32: {
    auto s = llvm::StructType::create(context, "string32");
    s->setBody({
        llvm::PointerType::get(context, 0), // data
        llvm::Type::getInt64Ty(context),    // len
        llvm::Type::getInt64Ty(context),
    });
    auto *fn =
        getRuntimeFunc("hrd_destroy_s32",
                       llvm::FunctionType::get(builder.getVoidTy(),
                                               {builder.getPtrTy()}, false));
    defaultDestroys.emplace(type, fn);
    return s;
  }
  case BuiltInType::B:
    return llvm::Type::getInt1Ty(context);
  case BuiltInType::FI: {
    Error::internal("yet supported fixedTypei");
  case BuiltInType::VOID: {
    Error::internal("illegal typeSymbol");
  }
  }
  }
  Error::internal("illegal typeSymbol");
}

llvm::Type *llvmCodegen::buildArrayType(ArrayTypeSymbol *arr) {
  auto found = types.find(arr);
  if (found != types.end()) {
    return found->second;
  }

  llvm::Type *baseTy = nullptr;

  if (arr->baseType->kind == TypeSymbol::TypeKind::ARRAY) {
    auto *inner = dynamic_cast<ArrayTypeSymbol *>(arr->baseType);
    if (inner == nullptr) {
      Error::internal("illegal array base type");
    }

    baseTy = buildArrayType(inner);
  } else {
    baseTy = getLayoutType(arr->baseType);
  }

  uint64_t len = arr->sizeValue.getZExtValue();
  auto *arrayTy = llvm::ArrayType::get(baseTy, len);

  types.emplace(arr, arrayTy);
  return arrayTy;
}

void llvmCodegen::declareArrayDestroy(ArrayTypeSymbol *arr) {
  if (!needsDestroy(arr)) {
    return;
  }

  if (defaultDestroys.find(arr) != defaultDestroys.end())
    return;

  auto *fnTy =
      llvm::FunctionType::get(builder.getVoidTy(), {builder.getPtrTy()}, false);

  auto *fn = llvm::Function::Create(fnTy, llvm::Function::InternalLinkage,
                                    getArrayDestroyName(arr), llvmModule.get());

  defaultDestroys.emplace(arr, fn);
}

void llvmCodegen::emitArrayDestroy(ArrayTypeSymbol *arr) {
  if (!needsDestroy(arr)) {
    return;
  }

  if (arr->baseType->kind == TypeSymbol::TypeKind::ARRAY) {
    auto *inner = dynamic_cast<ArrayTypeSymbol *>(arr->baseType);
    if (inner == nullptr) {
      Error::internal("illegal nested array type");
    }

    emitArrayDestroy(inner);
  }

  auto fn = defaultDestroys.at(arr);

  auto *entry = llvm::BasicBlock::Create(context, "entry", fn);
  builder.SetInsertPoint(entry);

  auto *arrayPtr = fn->getArg(0);
  auto *arrayTy = llvm::cast<llvm::ArrayType>(getLayoutType(arr));

  auto *destroyFn = defaultDestroys.at(arr->baseType);

  uint64_t len = arr->sizeValue.getZExtValue();

  for (uint64_t i = 0; i < len; ++i) {
    auto *elemPtr = builder.CreateInBoundsGEP(
        arrayTy, arrayPtr, {builder.getInt32(0), builder.getInt64(i)});

    builder.CreateCall(destroyFn, {elemPtr});
  }

  builder.CreateRetVoid();
}

std::string llvmCodegen::getArrayDestroyName(ArrayTypeSymbol *arr) {
  return "__destroy.array." + arr->baseType->name + "." +
         std::to_string(arr->sizeValue.getZExtValue());
}