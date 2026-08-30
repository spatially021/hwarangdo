
#include "hrd/IR/MIR/MIRExpr.h"
#include "hrd/IR/MIR/MIRStmt.h"
#include "hrd/IR/llvmIR/llvmCodegen.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/util/Error.h"
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
void llvmCodegen::lowerStmt(MIRStmt *stmt, FuncContext &ctx) {
  if (auto expr = dynamic_cast<MIRExprStmt *>(stmt)) {
    lowerExprStmt(expr, ctx);
    return;
  }
  if (auto assign = dynamic_cast<MIRAssignStmt *>(stmt)) {
    lowerAssign(assign, ctx);
    return;
  }
  if (auto local = dynamic_cast<MIRLocalDeclStmt *>(stmt)) {
    lowerLocalDecl(local, ctx);
    return;
  }
  if (auto quit = dynamic_cast<MIRQuitStmt *>(stmt)) {
    lowerQuit(quit, ctx);
    return;
  }
  if (auto destroy = dynamic_cast<MIRDestroyStmt *>(stmt)) {
    lowerDestroy(destroy, ctx);
    return;
  }

  if (auto cleanup = dynamic_cast<MIRCleanupStmt *>(stmt)) {
    lowerCleanup(cleanup, ctx);
    return;
  }

  Error::internal("illegal stmt kind");
}

void llvmCodegen::lowerExprStmt(MIRExprStmt *stmt, FuncContext &ctx) {

  (void)lowerValue(stmt->expr.get(), ctx);
}

void llvmCodegen::lowerCleanup(MIRCleanupStmt *stmt, FuncContext &ctx) {
  auto locals = stmt->locals;

  for (auto it = locals.rbegin(); it != locals.rend(); ++it) {

    ValueSymbol *sym = *it;
    if (!needsDestroy(sym->typeSymbol)) {
      continue;
    }
    {
      auto i = ctx.locals.find(sym);
      if (i == ctx.locals.end()) {
        Error::internal("fail to find local : " + sym->name);
      }
    }
    llvm::Value *slot = ctx.locals.at(sym);
    auto i = defaultDestroys.find(sym->typeSymbol);
    if (i == defaultDestroys.end()) {
      Error::internal("fail to find default destroy : " + sym->name + "[" +
                      sym->typeSymbol->name + "]");
    }
    auto *destroy = i->second;
    builder.CreateCall(destroy, {slot});
  }
}

void llvmCodegen::lowerAssign(MIRAssignStmt *stmt, FuncContext &ctx) {
  auto dst = lowerPlace(stmt->lhs.get(), ctx);
  auto rhs = lowerValue(stmt->rhs.get(), ctx);
  auto *ty = stmt->rhs->type;
  assign(dst, rhs, ty, ctx);
}

llvm::AllocaInst *llvmCodegen::createEntryAlloca(llvm::Function *fn,
                                                 llvm::Type *ty,
                                                 llvm::StringRef name) {
  llvm::IRBuilder<> tmp(&fn->getEntryBlock(),
                        fn->getEntryBlock().getFirstInsertionPt());

  return tmp.CreateAlloca(ty, nullptr, name);
}

void llvmCodegen::lowerQuit(MIRQuitStmt *, FuncContext &) {
  auto quitFn =
      getRuntimeFunc("hrd_world_quit",
                     llvm::FunctionType::get(builder.getVoidTy(), {}, false));
  builder.CreateCall(quitFn);
}
