#include "IR/HIR/HIRNode.h"
#include "IR/HIR/HIRStmt.h"
#include "IR/MIR/MIRBuilder.h"
#include "IR/MIR/MIRExpr.h"
#include "IR/MIR/MIRInst.h"
#include "IR/MIR/MIRNode.h"
#include <memory>
#include <utility>

void MIRBuilder::lowerBlock(HIRBlockStmt *block) {
  for (auto &s : block->statements) {
    if (hasTerminator(currentBlock)) {
      break; // 또는 unreachable block 생성/경고 처리
    }

    lowerStmt(s.get());
  }
}

void MIRBuilder::lowerStmt(HIRStmt *stmt) {

  switch (stmt->kind) {

  case HIRNodeKind::BlockStmt: {
    auto blockStmt = expect<HIRBlockStmt>(stmt, HIRNodeKind::BlockStmt);
    lowerBlock(blockStmt);
    break;
  }
  case HIRNodeKind::ExprStmt: {
    auto experStmt = expect<HIRExprStmt>(stmt, HIRNodeKind::ExprStmt);
    lowerExprStmt(experStmt);
    break;
  }
  case HIRNodeKind::LocalDeclStmt: {
    break;
  }
  case HIRNodeKind::MethodDeclStmt: {
    break;
  }
  case HIRNodeKind::IfStmt: {
    auto ifStmt = expect<HIRIfStmt>(stmt, HIRNodeKind::IfStmt);
    lowerIf(ifStmt);
    break;
  }
  case HIRNodeKind::WhileStmt: {
    auto whileStmt = expect<HIRWhileStmt>(stmt, HIRNodeKind::WhileStmt);
    lowerWhile(whileStmt);
    break;
  }
  case HIRNodeKind::ForRangeStmt: {
    auto forRangeStmt =
        expect<HIRForRangeStmt>(stmt, HIRNodeKind::ForRangeStmt);
    lowerForRange(forRangeStmt);
    break;
  }
  case HIRNodeKind::ReturnStmt: {
    break;
  }
  case HIRNodeKind::BreakStmt: {
    break;
  }
  case HIRNodeKind::ContinueStmt: {
    break;
  }
  case HIRNodeKind::SwitchStmt: {
    break;
  }
  case HIRNodeKind::Case: {
    break;
  }
  case HIRNodeKind::OnExitStmt: {
    break;
  }
  case HIRNodeKind::ValueTransferStmt: {
    break;
  }
  case HIRNodeKind::DestroyStmt: {
    break;
  }
  case HIRNodeKind::QuitStmt: {
    break;
  }
  case HIRNodeKind::AssignStmt: {
    auto assign = expect<HIRAssignStmt>(stmt, HIRNodeKind::AssignStmt);
    lowerAssign(assign);
    break;
  }
  case HIRNodeKind::CompoundAssignStmt: {
    auto compound =
        expect<HIRCompoundAssignStmt>(stmt, HIRNodeKind::CompoundAssignStmt);
    lowerCompoundAssign(compound);
    break;
  }
  default:
    Error::internal(stmt->span, "illegal hir kind");
    break;
  }
}

void MIRBuilder::lowerExprStmt(HIRExprStmt *stmt) {
  unique_ptr<MIRExpr> expr = lowerExpr(stmt->expr.get());
  emit(make_unique<MIRExprStmtInst>(std::move(expr)));
}

void MIRBuilder::lowerIf(HIRIfStmt *stmt) {
  unique_ptr<MIRExpr> condExpr = lowerExpr(stmt->condition.get());

  BlockID cond = currentBlock;
  BlockID then = makeBlock();
  BlockID else_ = makeBlock();
  BlockID join = makeBlock();

  getBlock(cond)->terminator =
      BranchTerminator(std::move(condExpr), then, else_);

  currentBlock = then;
  lowerBlock(stmt->thenBlock.get());
  if (!hasTerminator(currentBlock)) {
    getBlock(currentBlock)->terminator = GotoTerminator(join);
  }

  currentBlock = else_;
  if (stmt->elseBlock) {
    lowerBlock(stmt->elseBlock.get());
  }
  if (!hasTerminator(else_)) {
    getBlock(currentBlock)->terminator = GotoTerminator(join);
  }

  currentBlock = join;
}

void MIRBuilder::lowerWhile(HIRWhileStmt *stmt) {
  unique_ptr<MIRExpr> condExpr = lowerExpr(stmt->condition.get());

  BlockID cond = currentBlock;
  BlockID then = makeBlock();
  BlockID else_ = makeBlock();
  BlockID join = makeBlock();

  getBlock(cond)->terminator =
      BranchTerminator(std::move(condExpr), then, else_);

  currentBlock = then;
  lowerBlock(stmt->body.get());
  if (!hasTerminator(currentBlock)) {
    getBlock(currentBlock)->terminator = GotoTerminator(cond);
  }

  getBlock(else_)->terminator = GotoTerminator(join);
  currentBlock = join;
}

void MIRBuilder::lowerForRange(HIRForRangeStmt *stmt) {
  // TODO: 표현식 끝내고 다시 하기. 그냥 while응용임
}

void MIRBuilder::lowerAssign(HIRAssignStmt *stmt) {}

void MIRBuilder::lowerCompoundAssign(HIRCompoundAssignStmt *stmt) {}