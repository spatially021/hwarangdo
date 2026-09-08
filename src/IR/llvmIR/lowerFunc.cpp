#include "hrd/IR/llvmIR/llvmCodegen.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/util/Error.h"

void llvmCodegen::buildMethods() {
  for (auto &m : program->functions) {

    if (m->isDefaultInit) {
      auto *voidTy = getType(table.registry.getBuilt("void"));
      auto *selfTy = llvm::PointerType::get(context, 0);

      auto *fnType = llvm::FunctionType::get(voidTy, {selfTy}, false);

      auto *fn = llvm::Function::Create(
          fnType, llvm::GlobalValue::ExternalLinkage,
          m->owner->name + "default.init.field", llvmModule.get());
      defaultInits.emplace(m->owner, fn);
      continue;
    }

    auto fn = createFuncShell(m->symbol);
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

      FuncContext ctx = {fn,   blocks,   locals,       params,
                         self, m->owner, cleanupStack, canceledCleanups};

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
    if (!m->symbol->isExtern) {
      emitFuncBody(m.get());
    }
  }
}

string llvmCodegen::mangle(MethodSymbol *symbol) {
  if (symbol->isExtern) {
    return symbol->linkName;
  }
  string name = "__hrd_" + symbol->module->name;
  for (auto &p : symbol->path.segments) {
    name += "_" + p;
  }
  name += "_" + symbol->owner->name + "_" + symbol->name;

  for (auto *p : symbol->params) {
    name += "_" + p->typeSymbol->name;
  }

  return name;
}

string llvmCodegen::mangleType(TypeSymbol *type) {
  if (type->kind == TypeKind::BUILTIN)
    return type->name;

  if (type->module == nullptr) {
    Error::internal("type's module is nullptr : " + type->name);
  }
  string name = type->module->name;

  for (auto &p : type->path.segments)
    name += "_" + p;

  name += "_" + type->name;

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
