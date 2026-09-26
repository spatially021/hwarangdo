#include "hrd/IR/llvmIR/llvmCodegen.h"

#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"

#include <llvm/IR/Value.h>

#include <stdexcept>

llvm::Function *llvmCodegen::emitDefaultDestroy(TypeSymbol *ty) {

  auto fn = defaultDestroys.at(ty);

  auto *layoutTy = getLayoutType(ty);

  auto *bb = llvm::BasicBlock::Create(context, "entry", fn);

  builder.SetInsertPoint(bb);

  auto *self = fn->getArg(0);

  TypeSymbol *objectType = ty;

  unordered_map<GenericParamSymbol *, TypeSymbol *> substitution;
  bool isGeneric = false;

  if (auto *generic = dynamic_cast<GenericSymbol *>(ty)) {

    objectType = generic->origin;

    auto &params = generic->origin->getGenericParams();

    if (params.size() != generic->args.size()) {
      Error::internal("generic argument count mismatch: " + generic->name);
    }

    for (size_t i = 0; i < generic->args.size(); ++i) {
      substitution.emplace(params[i], generic->args[i]);
    }

    isGeneric = true;
  }

  if (auto *obj = dyn_cast<ObjectType>(objectType)) {

    for (auto it = obj->fields.rbegin(); it != obj->fields.rend(); ++it) {

      auto *field = *it;

      auto *fieldType =
          isGeneric ? resolveGenericType(field->typeSymbol, substitution)
                    : field->typeSymbol;

      auto *fieldPtr =
          builder.CreateStructGEP(layoutTy, self, field->index, field->name);

      if (!needsDestroy(fieldType)) {
        continue;
      }

      auto it_ = defaultDestroys.find(fieldType);

      if (it_ == defaultDestroys.end()) {
        throw runtime_error("fail to find default destroy : " + ty->name + "." +
                            field->name);
      }

      builder.CreateCall(it_->second, {fieldPtr});
    }
  }

  builder.CreateRetVoid();

  return fn;
}

void llvmCodegen::emitFuncBody(MIRFunction *func) {

  auto fn = funcs.at(func->symbol);

  auto entry = llvm::BasicBlock::Create(context, "entry", fn);

  builder.SetInsertPoint(entry);

  unordered_map<BlockID, llvm::BasicBlock *> blocks;

  for (auto &b : func->blocks) {

    blocks.emplace(b->id, llvm::BasicBlock::Create(
                              context, "bb" + std::to_string(b->id), fn));
  }

  localMap locals;

  ParamMap params;

  std::vector<Cleanup> cleanupStack;

  std::unordered_set<llvm::Value *> canceledCleanups;

  auto argIt = fn->arg_begin();

  llvm::Argument *self = nullptr;

  if (needSelf(func)) {

    self = &*argIt++;

    self->setName("self");

    params.emplace(func->symbol->selfReceiver, self);
  }

  FuncContext ctx = {fn,          blocks,       locals,           params, self,
                     func->owner, cleanupStack, canceledCleanups, nullptr};

  for (auto *p : func->symbol->params) {

    llvm::Argument *arg = &*argIt++;

    arg->setName(p->name);

    auto *slot = createEntryAlloca(fn, getType(p->typeSymbol), p->name);

    builder.CreateStore(arg, slot);

    ctx.params.emplace(p, slot);
  }

  builder.CreateBr(blocks.at(func->entry));

  for (auto &b : func->blocks) {
    lowerBlock(b.get(), ctx);
  }
}

void llvmCodegen::emitFieldZeroInit(TypeSymbol *owner, llvm::Value *self) {

  auto *layoutTy = getLayoutType(owner);

  TypeSymbol *objectType = owner;

  unordered_map<GenericParamSymbol *, TypeSymbol *> substitution;
  bool isGeneric = false;

  if (auto *generic = dynamic_cast<GenericSymbol *>(owner)) {

    objectType = generic->origin;

    auto &params = generic->origin->getGenericParams();

    if (params.size() != generic->args.size()) {
      Error::internal("generic argument count mismatch: " + generic->name);
    }

    for (size_t i = 0; i < generic->args.size(); ++i) {
      substitution.emplace(params[i], generic->args[i]);
    }

    isGeneric = true;
  }

  auto *obj = dyn_cast<ObjectType>(objectType);

  if (obj == nullptr) {
    Error::internal("field zero init owner is not object type: " + owner->name);
  }

  for (auto *field : obj->fields) {

    auto *fieldType = isGeneric
                          ? resolveGenericType(field->typeSymbol, substitution)
                          : field->typeSymbol;

    auto *fieldPtr = builder.CreateStructGEP(layoutTy, self, field->index,
                                             field->name + ".zero");

    if (needsDestroy(fieldType)) {

      builder.CreateStore(llvm::Constant::getNullValue(getType(fieldType)),
                          fieldPtr);
    }

    if (fieldType->kind == TypeKind::STRUCT) {

      auto it = defaultInits.find(fieldType);

      if (it == defaultInits.end()) {
        Error::internal("fail to find default init: " + fieldType->name);
      }

      builder.CreateCall(it->second, {fieldPtr});
    }
  }
}

void llvmCodegen::addClean(llvm::Value *addr, TypeSymbol *type,
                           FuncContext &ctx) {

  auto *resolvedType = resolveType(type, ctx);

  ctx.cleanupStack.push_back({addr, resolvedType});
}