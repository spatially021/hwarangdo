#include "hrd/IR/llvmIR/CodegenStructs.h"
#include "hrd/IR/llvmIR/llvmCodegen.h"
#include "hrd/util/Error.h"
#include <cstddef>
#include <iostream>
#include <vector>

llvm::Type *llvmCodegen::getOrCreateGeneric(GenericSymbol *symbol) {
  llvm::IRBuilderBase::InsertPointGuard insertPointGuard(builder);

  if (auto it = genericMap.find(symbol); it != genericMap.end()) {
    return it->second;
  }

  auto *type = llvm::StructType::create(context, mangleType(symbol));
  genericMap.emplace(symbol, type);

  // Register destroy before resolving fields so recursive generic layouts can
  // refer back to this specialization.
  {
    auto *fnTy = llvm::FunctionType::get(builder.getVoidTy(),
                                         {builder.getPtrTy()}, false);

    auto *fn = llvm::Function::Create(fnTy, llvm::Function::LinkOnceODRLinkage,
                                      mangleType(symbol) + "destroy.field",
                                      llvmModule.get());

    auto [it, inserted] = defaultDestroys.emplace(symbol, fn);
    if (!inserted) {
      std::cerr << "duplicate destroy: " << symbol->name << "\n";
    }
  }

  unordered_map<GenericParamSymbol *, TypeSymbol *> substitution;
  auto *origin = symbol->origin;
  auto &genericParams = origin->getGenericParams();
  if (genericParams.size() != symbol->args.size()) {
    Error::internal("generic argument count mismatch: " + symbol->name);
  }

  for (size_t i = 0; i < symbol->args.size(); ++i) {
    substitution.emplace(genericParams[i], symbol->args[i]);
  }

  if (auto *obj = dynamic_cast<ObjectType *>(origin)) {
    vector<llvm::Type *> fields;
    fields.reserve(obj->fields.size());

    for (auto *field : obj->fields) {
      auto *resolved = resolveGenericType(field->typeSymbol, substitution);

      fields.push_back(getFieldType(resolved));
    }

    type->setBody(fields);

  } else if (dynamic_cast<EnumType *>(origin)) {
    vector<llvm::Type *> fields;

    fields.push_back(builder.getInt32Ty()); // tag
    fields.push_back(builder.getPtrTy());   // payload

    type->setBody(fields);
    emitDefaultDestroy(symbol);

    return type;
  }
  auto originIt = program->genericMap.find(origin);
  if (originIt == program->genericMap.end()) {
    Error::internal("generic MIR origin not found: " + symbol->name);
  }

  auto &genericOrigin = originIt->second;
  auto *defaultInitOrigin = genericOrigin.defaultInit;
  auto &methods = genericOrigin.methods;

  // Pass 1: declare every function shell before lowering any body. This is
  // required for recursion and calls to methods declared later in the type.
  if (defaultInitOrigin != nullptr) {
    auto *voidTy = getType(table.registry.getBuilt("void"));
    auto *selfTy = llvm::PointerType::get(context, 0);
    auto *fnType = llvm::FunctionType::get(voidTy, {selfTy}, false);

    auto *fn = llvm::Function::Create(
        fnType, llvm::GlobalValue::LinkOnceODRLinkage,
        mangleType(symbol) + "default.init.field", llvmModule.get());

    defaultInits.emplace(symbol, fn);
  }

  for (auto *m : methods) {
    auto *method = m->symbol;
    if (method->isGenericDecl) {
      continue;
    }
    bool hasSelf = needSelf(method);

    auto *returnType =
        getType(resolveGenericType(method->returnType, substitution));
    if (returnType == nullptr) {
      Error::internal("null llvm return type: " + symbol->name);
    }

    vector<llvm::Type *> params;
    if (hasSelf) {
      params.push_back(builder.getPtrTy());
    }

    for (auto *param : method->params) {
      auto *paramType =
          getType(resolveGenericType(param->typeSymbol, substitution));
      if (paramType == nullptr) {
        Error::internal("null llvm param type: " + param->name);
      }
      params.push_back(paramType);
    }

    auto *fnType = llvm::FunctionType::get(returnType, params, false);
    auto key = GenericMethodKey(symbol, method, vector<TypeSymbol *>());
    auto *fn =
        llvm::Function::Create(fnType, llvm::GlobalValue::LinkOnceODRLinkage,
                               mangle(method, key), llvmModule.get());

    genericMethodMap.emplace(key, fn);
  }

  // Layout and all dependent destroy declarations now exist.
  emitDefaultDestroy(symbol);

  // Pass 2: lower default-init body.
  if (defaultInitOrigin != nullptr) {
    auto *m = defaultInitOrigin;
    auto *fn = defaultInits.at(symbol);
    auto *entry = llvm::BasicBlock::Create(context, "initentry", fn);
    builder.SetInsertPoint(entry);

    unordered_map<BlockID, llvm::BasicBlock *> blocks;
    for (auto &block : m->blocks) {
      auto [_, inserted] = blocks.emplace(
          block->id, llvm::BasicBlock::Create(
                         context, "bb" + std::to_string(block->id), fn));
      if (!inserted) {
        Error::internal("fail to add block");
      }
    }

    localMap locals;
    ParamMap params;
    std::vector<Cleanup> cleanupStack;
    std::unordered_set<llvm::Value *> canceledCleanups;

    auto argIt = fn->arg_begin();
    llvm::Argument *self = &*argIt++;
    self->setName("self");

    FuncContext ctx = {fn,           blocks, locals,       params,
                       self,         symbol, cleanupStack, canceledCleanups,
                       &substitution};

    emitFieldZeroInit(symbol, self);

    for (auto &block : m->blocks) {
      lowerBlock(block.get(), ctx);
    }

    if (entry->getTerminator() == nullptr) {
      builder.SetInsertPoint(entry);
      builder.CreateBr(ctx.blocks.at(m->blocks.front()->id));
    }
  }

  // Pass 2: lower method bodies using the shells created above.
  for (auto *m : methods) {
    if (m->symbol->isGenericDecl) {
      continue;
    }
    auto *method = m->symbol;
    bool hasSelf = needSelf(method);
    auto *fn = genericMethodMap.at(
        GenericMethodKey(symbol, method, vector<TypeSymbol *>()));

    auto *entry = llvm::BasicBlock::Create(context, "genericentry", fn);
    builder.SetInsertPoint(entry);

    unordered_map<BlockID, llvm::BasicBlock *> blocks;
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
      paramMap.emplace(method->selfReceiver, self);
    }

    FuncContext ctx = {fn,           blocks, locals,       paramMap,
                       self,         symbol, cleanupStack, canceledCleanups,
                       &substitution};

    for (auto *param : method->params) {
      llvm::Argument *arg = &*argIt++;
      arg->setName(param->name);

      auto *paramType =
          getType(resolveGenericType(param->typeSymbol, substitution));
      auto *slot = createEntryAlloca(fn, paramType, param->name);
      builder.CreateStore(arg, slot);
      ctx.params.emplace(param, slot);
    }

    builder.CreateBr(blocks.at(m->entry));
    for (auto &block : m->blocks) {
      lowerBlock(block.get(), ctx);
    }
  }

  return type;
}

TypeSymbol *llvmCodegen::resolveGenericType(
    TypeSymbol *type,
    unordered_map<GenericParamSymbol *, TypeSymbol *> &substitution) {

  if (auto *param = dynamic_cast<GenericParamSymbol *>(type)) {
    auto it = substitution.find(param);

    if (it == substitution.end()) {
      Error::internal("unknown generic param: " + param->name);
    }

    return it->second;
  }

  if (auto *generic = dynamic_cast<GenericSymbol *>(type)) {
    vector<TypeSymbol *> args;
    args.reserve(generic->args.size());

    bool changed = false;

    for (auto *arg : generic->args) {
      auto *resolved = resolveGenericType(arg, substitution);

      args.push_back(resolved);

      if (resolved != arg) {
        changed = true;
      }
    }

    if (!changed) {
      return generic;
    }

    return table.registry.getOrCreateGeneric(generic->origin, args);
  }

  if (auto *arr = dynamic_cast<ArrayTypeSymbol *>(type)) {
    auto *base = resolveGenericType(arr->baseType, substitution);

    if (base == arr->baseType) {
      return arr;
    }

    return table.registry.getOrCreateArray(base, arr->sizeValue);
  }

  /*
   * Generic declaration 자체가 type으로 들어온 경우.
   *
   * self 같은 경우:
   *
   *   vec<T> -> vec<int>
   */
  if (auto *object = dynamic_cast<ObjectType *>(type)) {
    auto &params = object->getGenericParams();
    if (!params.empty()) {
      vector<TypeSymbol *> args;
      args.reserve(params.size());

      for (auto *param : params) {
        auto it = substitution.find(param);

        if (it == substitution.end()) {
          /*
           * 이 함수가 generic instantiation context에서만 호출된다면
           * internal error로 처리해도 됨.
           */
          Error::internal("unknown generic param: " + param->name);
        }

        args.push_back(it->second);
      }

      return table.registry.getOrCreateGeneric(object, args);
    }
  }

  return type;
}

TypeSymbol *llvmCodegen::resolveMemberType(TypeSymbol *type,
                                           TypeSymbol *ownerType,
                                           FuncContext &ctx) {
  ownerType = resolveType(ownerType, ctx);

  auto *generic = dynamic_cast<GenericSymbol *>(ownerType);

  if (generic == nullptr) {
    return resolveType(type, ctx);
  }

  std::unordered_map<GenericParamSymbol *, TypeSymbol *> substitution;

  auto params = generic->origin->getGenericParams();
  auto &args = generic->args;

  if (params.size() != args.size()) {
    Error::internal("generic param/arg size mismatch");
  }

  for (size_t i = 0; i < params.size(); ++i) {
    auto *arg = resolveType(args[i], ctx);
    substitution.emplace(params[i], arg);
  }

  return resolveGenericType(type, substitution);
}