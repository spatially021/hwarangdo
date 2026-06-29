
#include "hrd/IR/MIR/MIRStmt.h"
#include "hrd/IR/llvmIR/llvmCodegen.h"
#include "hrd/util/Error.h"
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
  }
  if (auto destroy = dynamic_cast<MIRDestroyStmt *>(stmt)) {
  }
  Error::internal("illegal stmt kind");
}

void llvmCodegen::lowerExprStmt(MIRExprStmt *stmt, FuncContext &ctx) {

  (void)lowerValue(stmt->expr.get(), ctx);
}

void llvmCodegen::lowerAssign(MIRAssignStmt *stmt, FuncContext &ctx) {
  auto ptr = lowerPlace(stmt->lhs.get(), ctx);
  auto value = lowerValue(stmt->rhs.get(), ctx);

  builder.CreateStore(value, ptr);
}

void llvmCodegen::lowerLocalDecl(MIRLocalDeclStmt *stmt, FuncContext &ctx) {
  llvm::Type *ty = getType(stmt->type);
  llvm::AllocaInst *slot = createEntryAlloca(ctx.func, ty, stmt->symbol->name);
  ctx.locals.emplace(stmt->symbol, slot);

  if (stmt->init) {
    llvm::Value *init = lowerValue(stmt->init.get(), ctx);
    builder.CreateStore(init, slot);
  }
}

llvm::AllocaInst *llvmCodegen::createEntryAlloca(llvm::Function *fn,
                                                 llvm::Type *ty,
                                                 llvm::StringRef name) {
  llvm::IRBuilder<> tmp(&fn->getEntryBlock(),
                        fn->getEntryBlock().getFirstInsertionPt());

  return tmp.CreateAlloca(ty, nullptr, name);
}