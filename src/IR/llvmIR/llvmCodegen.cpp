#include "hrd/IR/llvmIR/llvmCodegen.h"
#include "hrd/AST/Decl.h"
#include "hrd/IR/MIR/MIRNode.h"
#include "hrd/SemanticAnalyzer/ResolvedLit.h"
#include "hrd/SemanticAnalyzer/SymbolTable/SymbolTable.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/compiler/CompilerContexts.h"
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
#include <variant>

template <class... Ts> struct Overloaded : Ts... {
  using Ts::operator()...;
};

template <class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

llvmCodegen::llvmCodegen(CodegenContext &ctx)
    : context(), program(ctx.program), table(ctx.table),
      llvmModule(make_unique<llvm::Module>("hwarangdo", context)),
      builder(context), isCompile(ctx.isCompile) {}

void llvmCodegen::generate() {
  buildTypes();
  declareRoots(table.scopeManger.getRootScope());
  buildMethods();
  if (!isCompile) {
    generateEntryMain(table.main);
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
                   auto cond = lowerValue(t.cond.get(), ctx);

                   builder.CreateCondBr(cond.value, ctx.blocks.at(t.trueBlock),
                                        ctx.blocks.at(t.falseBlock));
                 },

                 [&](const ReturnTerminator &t) {
                   //  emitCleanups(ctx);
                   if (t.value) {
                     builder.CreateRet(lowerValue(t.value.get(), ctx).value);
                   } else {
                     builder.CreateRetVoid();
                   }
                 },

                 [&](const SwitchTerminator &t) {
                   auto type = t.cond->type;
                   if (isString(type)) {
                     lowerStringSwitch(t, ctx);
                     return;
                   }
                   if (dynamic_cast<PrimtiveType *>(type)) {
                     lowerNativeSwitch(t, ctx);
                     return;
                   }
                   if (type->kind == TypeSymbol::TypeKind::ENUM) {
                     lowerEnumSwitch(t, ctx);
                     return;
                   }
                   Error::internal("unknwon cond kind");
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
