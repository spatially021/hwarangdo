#include "hrd/IR/llvmIR/llvmCodegen.h"

void llvmCodegen::buildMethods() {
  for (auto &m : program->functions) {

    if (m->isDefaultInit) {
      auto *voidTy = getType(table.getType("void"));
      auto *selfTy = llvm::PointerType::get(context, 0);

      auto *fnType = llvm::FunctionType::get(voidTy, {selfTy}, false);

      auto *fn = llvm::Function::Create(
          fnType, llvm::GlobalValue::ExternalLinkage,
          m->owner->name + "default.init.field", llvmModule.get());
      defaultInits.emplace(m->owner, fn);
      continue;
    }

    auto rt = getType(m->symbol->returnType);
    if (rt == nullptr) {
      Error::internal("null llvm return type: " + m->symbol->name);
    }

    vector<llvm::Type *> params;
    params.push_back(llvm::PointerType::get(context, 0));
    for (auto &p : m->symbol->params) {
      auto pt = getType(p->typeSymbol);
      if (pt == nullptr) {
        Error::internal("null llvm param type: " + p->name);
      }
      params.push_back(pt);
    }
    auto fnType = llvm::FunctionType::get(rt, params, false);
    auto fn = llvm::Function::Create(fnType, llvm::GlobalValue::ExternalLinkage,
                                     mangle(m->symbol), llvmModule.get());
    funcs.emplace(m->symbol, fn);
  }

  for (auto &m : program->functions) {
    if (m->isDefaultInit) {
      auto fn = defaultInits.at(m->owner);
      auto entry = llvm::BasicBlock::Create(context, "entry", fn);
      unordered_map<BlockID, llvm::BasicBlock *> blocks;

      for (auto &b : m->blocks) {
        blocks.emplace(b->id, llvm::BasicBlock::Create(
                                  context, "bb" + std::to_string(b->id), fn));
      }

      builder.SetInsertPoint(entry);

      localMap locals;
      ParamMap params;
      std::vector<Cleanup> cleanupStack;
      std::unordered_set<llvm::Value *> canceledCleanups;
      auto argIt = fn->arg_begin();

      llvm::Argument *self = &*argIt++;
      self->setName("self");

      FuncContext ctx = {fn,   blocks,       locals,          params,
                         self, cleanupStack, canceledCleanups};

      emitFieldZeroInit(m->owner, self);

      for (auto &s : m->blocks) {
        lowerBlock(s.get(), ctx);
      }

      if (entry->getTerminator() == nullptr) {
        builder.SetInsertPoint(entry);
        builder.CreateBr(ctx.blocks.at(m->blocks.front()->id));
      }

      continue;
    }

    emitFuncBody(m.get());
  }
}

string llvmCodegen::mangle(MethodSymbol *symbol) {
  string name = symbol->owner->name + "_" + symbol->name;

  for (auto *p : symbol->params) {
    name += "_" + p->typeSymbol->name;
  }

  return name;
}

llvm::Function *llvmCodegen::getRuntimeFunc(const std::string &name,
                                            llvm::FunctionType *type) {
  if (auto *fn = llvmModule->getFunction(name)) {
    if (fn->getFunctionType() != type) {
      Error::internal("runtime function type mismatch: " + name);
    }
    return fn;
  }

  return llvm::Function::Create(type, llvm::Function::ExternalLinkage, name,
                                llvmModule.get());
}
