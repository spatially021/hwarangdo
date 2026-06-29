#include "hrd/IR/llvmIR/llvmCodegen.h"
#include "hrd/AST/Decl.h"
#include "hrd/BuiltInType.h"
#include "hrd/IR/MIR/MIRNode.h"
#include "hrd/IR/MIR/MIRProgram.h"
#include "hrd/SemanticAnalyzer/ResolvedLit.h"
#include "hrd/SemanticAnalyzer/SymbolTable.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/util/Error.h"
#include <llvm/IR/Argument.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/Support/Casting.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>

template <class... Ts> struct Overloaded : Ts... {
  using Ts::operator()...;
};

template <class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

llvmCodegen::llvmCodegen(MIRProgram *p, SymbolTable *t)
    : context(), program(p), table(t),
      llvmModule(make_unique<llvm::Module>("hwarangdo", context)),
      builder(context) {}

void llvmCodegen::generate() {

  buildTypes();
  declareRoots(table->rootScope.get());
  buildMethods();
  generateEntryMain(table->main);
}

void llvmCodegen::buildTypes() {
  for (auto &t : table->types) {

    if (auto p = dynamic_cast<PrimtiveType *>(t)) {
      types.emplace(t, buildPrimitiveType(p));
      continue;
    }

    switch (t->kind) {

    case TypeSymbol::TypeKind::VOID: {
      types.emplace(t, llvm::Type::getVoidTy(context));
    }
    case TypeSymbol::TypeKind::ARRAY:
    case TypeSymbol::TypeKind::FUNC:
    case TypeSymbol::TypeKind::HANDLE:
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
      continue;
    }
    if (t->kind == TypeSymbol::TypeKind::ENUM) {
    }
  }

  for (auto &t : table->types) {
    if (dynamic_cast<PrimtiveType *>(t)) {
      continue;
    }
    if (t->kind == TypeSymbol::TypeKind::CLASS ||
        t->kind == TypeSymbol::TypeKind::STRUCT) {

      auto type = llvm::dyn_cast<llvm::StructType>(getType(t));
      if (type == nullptr) {
        Error::internal("illegal llvm type");
      }

      vector<llvm::Type *> fields;
      for (auto &f : t->fields) {
        fields.push_back(getType(f->typeSymbol));
      }
      type->setBody(fields);
      continue;
    }
  }
}

llvm::Type *llvmCodegen::getType(TypeSymbol *t) {
  if (auto it = types.find(t); it != types.end()) {
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

llvm::Type *llvmCodegen::buildPrimitiveType(PrimtiveType *type) {
  if (type == nullptr) {
    Error::internal("falled to cast typeSymbol to primitiveType");
  }
  auto ptrTy = llvm::PointerType::getUnqual(context);
  auto i64Ty = llvm::Type::getInt64Ty(context);

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
    s->setBody({ptrTy, i64Ty});
    return s;
  }

  case BuiltInType::S16: {
    auto s = llvm::StructType::create(context, "string16");
    s->setBody({ptrTy, i64Ty});
    return s;
  }
  case BuiltInType::S32: {
    auto s = llvm::StructType::create(context, "string32");
    s->setBody({ptrTy, i64Ty});
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
}

void llvmCodegen::buildMethods() {
  for (auto &m : program->functions) {
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

  auto argIt = fn->arg_begin();

  llvm::Argument *self = &*argIt++;
  self->setName("self");

  params.emplace(func->symbol->selfReceiver, self);

  FuncContext ctx = {fn, blocks, locals, params};

  for (auto p : func->symbol->params) {
    llvm::Argument *arg = &*argIt++;
    arg->setName(p->name);

    auto *slot = createEntryAlloca(fn, getType(p->typeSymbol), p->name);
    builder.CreateStore(arg, slot);

    ctx.params.emplace(p, slot);
  }
  builder.CreateBr(blocks.at(func->entry)); // 또는 func->blocks.front()->id

  for (auto &b : func->blocks) {
    lowerBlock(b.get(), ctx);
  }

  if (entry->getTerminator() == nullptr) {
    builder.SetInsertPoint(entry);
    builder.CreateBr(ctx.blocks.at(func->blocks.front()->id));
  }
}

void llvmCodegen::lowerBlock(BasicBlock *block, FuncContext &ctx) {
  builder.SetInsertPoint(ctx.blocks.at(block->id));
  for (auto &s : block->stmts) {
    lowerStmt(s.get(), ctx);
  }

  lowerTerminator(block->terminator, ctx);
}

void llvmCodegen::lowerTerminator(MIRTerminator &terminator, FuncContext &ctx) {
  if (builder.GetInsertBlock()->getTerminator()) {
    Error::internal("LLVM block already has terminator");
  }

  std::visit(Overloaded{
                 [&](const std::monostate &) {
                   Error::internal("MIR block has no terminator");
                 },

                 [&](const GotoTerminator &t) {
                   builder.CreateBr(ctx.blocks.at(t.targetBlock));
                 },

                 [&](const BranchTerminator &t) {
                   llvm::Value *cond = lowerValue(t.cond.get(), ctx);

                   builder.CreateCondBr(cond, ctx.blocks.at(t.trueBlock),
                                        ctx.blocks.at(t.falseBlock));
                 },

                 [&](const ReturnTerminator &t) {
                   if (t.value) {
                     builder.CreateRet(lowerValue(t.value.get(), ctx));
                   } else {
                     builder.CreateRetVoid();
                   }
                 },

                 [&](const SwitchTerminator &t) {
                   llvm::Value *cond = lowerValue(t.cond.get(), ctx);

                   auto *defaultBB = ctx.blocks.at(t.defaultTarget);
                   if (t.cases.size() > std::numeric_limits<unsigned>::max()) {
                     Error::internal("too many switch cases");
                   }

                   auto *sw = builder.CreateSwitch(
                       cond, defaultBB, static_cast<unsigned>(t.cases.size()));

                   for (const auto &c : t.cases) {
                     if (auto literal = get_if<ResolvedLit>(&c.value)) {
                     }

                     // TOOD:이후에 구조 확립해서 구현하기.
                   }
                 },
             },
             terminator);
}
uint64_t llvmCodegen::arrayLengthToU64(const llvm::APInt &v) {
  if (v.isNegative()) {
    Error::internal("array length is negative");
  }

  if (v.getActiveBits() > 64) {
    Error::internal("array length is too large");
  }

  auto n = v.getZExtValue();

  if (n == 0) {
    Error::internal("array length must be greater than zero");
  }

  return n;
}

void llvmCodegen::declareRoots(Scope *rootScope) {
  for (auto &[name, symbol] : rootScope->value) {
    auto *ty = getType(symbol->typeSymbol);

    auto *g = new llvm::GlobalVariable(
        *llvmModule, ty, false, llvm::GlobalValue::ExternalLinkage,
        llvm::Constant::getNullValue(ty), "__hrd_root_" + symbol->name);

    roots.emplace(symbol.get(), g);
  }
}

MethodSymbol *llvmCodegen::getMainMethod(const string &name) {

  if (name == "update") {
    return table->main->update;
  }

  if (name == "init") {
    return table->main->init;
  }

  Error::internal("Main method not found: " + name);
}

void llvmCodegen::generateEntryMain(MainSymbol *mainType) {
  auto i32Ty = llvm::Type::getInt32Ty(context);
  // auto i1Ty = llvm::Type::getInt1Ty(context);
  // auto ptrTy = llvm::PointerType::getUnqual(context);

  auto mainFnTy = llvm::FunctionType::get(i32Ty, {}, false);
  auto mainFn = llvm::Function::Create(
      mainFnTy, llvm::Function::ExternalLinkage, "main", llvmModule.get());

  auto entryBB = llvm::BasicBlock::Create(context, "entry", mainFn);
  // auto loopCondBB = llvm::BasicBlock::Create(context, "loop.cond", mainFn);
  // auto loopBodyBB = llvm::BasicBlock::Create(context, "loop.body", mainFn);
  // auto exitBB = llvm::BasicBlock::Create(context, "exit", mainFn);

  builder.SetInsertPoint(entryBB);

  // // extern HrdWorld* hrd_world_create();
  // auto worldCreateFn = getRuntimeFunc(
  //     "hrd_world_create", llvm::FunctionType::get(ptrTy, {}, false));

  // // extern bool hrd_world_running(HrdWorld*);
  // auto worldRunningFn = getRuntimeFunc(
  //     "hrd_world_running", llvm::FunctionType::get(i1Ty, {ptrTy}, false));

  // // extern void hrd_world_destroy(HrdWorld*);
  // auto worldDestroyFn = getRuntimeFunc(
  //     "hrd_world_destroy",
  //     llvm::FunctionType::get(builder.getVoidTy(), {ptrTy}, false));

  // llvm::Value *world = builder.CreateCall(worldCreateFn);

  // 임시: Main 객체 생성 방식은 나중에 world.spawn으로 교체 가능
  llvm::Type *mainLlvmTy = getType(mainType);
  llvm::Value *mainObj = builder.CreateAlloca(mainLlvmTy);

  auto init = getMainMethod("init");
  auto mainUpdate = funcs.at(getMainMethod("update"));

  if (init) {
    auto mainInit = funcs.at(init);
    builder.CreateCall(mainInit, {mainObj});
  }
  builder.CreateCall(mainUpdate, {mainObj});

  /*  builder.CreateBr(loopCondBB);

   builder.SetInsertPoint(loopCondBB);
   llvm::Value *running = builder.CreateCall(worldRunningFn, {world});
   builder.CreateCondBr(running, loopBodyBB, exitBB);

   builder.SetInsertPoint(loopBodyBB);
   builder.CreateCall(mainUpdate, {mainObj});
   builder.CreateBr(loopCondBB);

   builder.SetInsertPoint(exitBB);
   builder.CreateCall(worldDestroyFn, {world});
   builder.CreateRet(builder.getInt32(0)); */
  builder.CreateRet(builder.getInt32(0));
}

llvm::Function *llvmCodegen::getRuntimeFunc(const std::string &name,
                                            llvm::FunctionType *type) {
  if (auto *fn = llvmModule->getFunction(name)) {
    return fn;
  }

  return llvm::Function::Create(type, llvm::Function::ExternalLinkage, name,
                                llvmModule.get());
}