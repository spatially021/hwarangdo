#include "hrd/AST/Stmt.h"
#include "hrd/IR/HIR/HIRNode.h"
#include "hrd/IR/HIR/HIRPattern.h"
#include "hrd/IR/HIR/HIRStmt.h"
#include "hrd/IR/MIR/MIRBuilder.h"
#include "hrd/IR/MIR/MIRExpr.h"
#include "hrd/IR/MIR/MIRNode.h"
#include "hrd/IR/MIR/MIRStmt.h"
#include "hrd/enums/Operator.h"
#include "hrd/util/Error.h"
#include <cassert>
#include <memory>
#include <utility>
#include <variant>

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
    auto local = expect<HIRLocalDeclStmt>(stmt, HIRNodeKind::LocalDeclStmt);
    loewrLocalDecl(local);
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
    auto ret = expect<HIRReturnStmt>(stmt, HIRNodeKind::ReturnStmt);
    lowerReturn(ret);
    break;
  }
  case HIRNodeKind::BreakStmt: {
    auto br = expect<HIRBreakStmt>(stmt, HIRNodeKind::BreakStmt);
    lowerBreak(br);
    break;
  }
  case HIRNodeKind::ContinueStmt: {
    auto con = expect<HIRContinueStmt>(stmt, HIRNodeKind::ContinueStmt);
    lowerContinue(con);
    break;
  }
  case HIRNodeKind::SwitchStmt: {
    auto swit = expect<HIRSwitchStmt>(stmt, HIRNodeKind::SwitchStmt);
    lowerSwitch(swit);
    break;
  }
  case HIRNodeKind::OnExitStmt: {
    break;
  }
  case HIRNodeKind::ValueTransferStmt: {
    auto transfer =
        expect<HIRValueTransferStmt>(stmt, HIRNodeKind::ValueTransferStmt);
    lowerValueTransfer(transfer);
    break;
  }
  case HIRNodeKind::DestroyStmt: {
    auto destroy = expect<HIRDestroyStmt>(stmt, HIRNodeKind::DestroyStmt);
    lowerDestroy(destroy);
    break;
  }
  case HIRNodeKind::QuitStmt: {
    auto quit = expect<HIRQuitStmt>(stmt, HIRNodeKind::QuitStmt);
    lowerQuit(quit);

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
  unique_ptr<MIRValue> expr = lowerExpr(stmt->expr.get());
  emit(make_unique<MIRExprStmt>(std::move(expr)));
}

void MIRBuilder::lowerIf(HIRIfStmt *stmt) {
  unique_ptr<MIRValue> condExpr = lowerExpr(stmt->condition.get());

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

  if (!hasTerminator(currentBlock)) {
    getBlock(currentBlock)->terminator = GotoTerminator(join);
  }

  currentBlock = join;
}

void MIRBuilder::lowerWhile(HIRWhileStmt *stmt) {
  unique_ptr<MIRValue> condExpr = lowerExpr(stmt->condition.get());

  BlockID cond = currentBlock;
  BlockID then = makeBlock();
  BlockID else_ = makeBlock();
  BlockID join = makeBlock();

  getBlock(cond)->terminator =
      BranchTerminator(std::move(condExpr), then, else_);

  currentBlock = then;
  LoopContext l = {cond, join};
  loops.push_back(l);
  lowerBlock(stmt->body.get());
  loops.pop_back();

  if (!hasTerminator(currentBlock)) {
    getBlock(currentBlock)->terminator = GotoTerminator(cond);
  }

  getBlock(else_)->terminator = GotoTerminator(join);
  currentBlock = join;
}

void MIRBuilder::lowerForRange(HIRForRangeStmt *stmt) {
  BlockID entry = currentBlock;
  BlockID cond = makeBlock();
  BlockID body = makeBlock();
  BlockID step_ = makeBlock();
  BlockID join = makeBlock();

  auto type = stmt->indexVar->type->typeSymbol;
  currentBlock = entry;
  auto symbol = stmt->indexVar->symbol;
  emit(make_unique<MIRLocalDeclStmt>(type, symbol, nullptr));
  emit(make_unique<MIRAssignStmt>(make_unique<MIRLocalPlace>(symbol),
                                  lowerExpr(stmt->start.get())));
  getBlock(entry)->terminator = GotoTerminator(cond);

  currentBlock = cond;
  unique_ptr<MIRBinaryExpr> condExpr = make_unique<MIRBinaryExpr>(
      make_unique<MIRLoad>(make_unique<MIRLocalPlace>(symbol), type),
      lowerExpr(stmt->end.get()), Operator(Operator::LS), type, type);
  getBlock(cond)->terminator =
      BranchTerminator(std::move(condExpr), body, join);

  currentBlock = body;
  LoopContext l = {step_, join};
  loops.push_back(l);
  lowerBlock(stmt->body.get());
  if (!hasTerminator(currentBlock)) {
    getBlock(currentBlock)->terminator = GotoTerminator(step_);
  }
  loops.pop_back();
  currentBlock = step_;

  emit(make_unique<MIRAssignStmt>(
      make_unique<MIRLocalPlace>(symbol),
      make_unique<MIRBinaryExpr>(
          make_unique<MIRLoad>(make_unique<MIRLocalPlace>(symbol), type),
          lowerExpr(stmt->step.get()), Operator::PLUS, type, type)));

  getBlock(step_)->terminator = GotoTerminator(cond);
  currentBlock = join;
}

void MIRBuilder::lowerAssign(HIRAssignStmt *stmt) {
  unique_ptr<MIRPlace> lhs = lowerPlace(stmt->lhs.get());
  unique_ptr<MIRValue> rhs = lowerExpr(stmt->rhs.get());
  emit(make_unique<MIRAssignStmt>(std::move(lhs), std::move(rhs)));
}

void MIRBuilder::lowerCompoundAssign(HIRCompoundAssignStmt *stmt) {
  unique_ptr<MIRPlace> lhs = lowerPlace(stmt->lhs.get());

  unique_ptr<MIRValue> rhs = make_unique<MIRBinaryExpr>(
      make_unique<MIRLoad>(lhs->clone(), lhs->symbol->typeSymbol),
      lowerExpr(stmt->rhs.get()), stmt->op, lhs->symbol->typeSymbol,
      lhs->symbol->typeSymbol);
  emit(make_unique<MIRAssignStmt>(std::move(lhs), std::move(rhs)));
}

void MIRBuilder::lowerQuit(HIRQuitStmt *) { emit(make_unique<MIRQuitStmt>()); }

void MIRBuilder::lowerBreak(HIRBreakStmt *) {
  getBlock(currentBlock)->terminator = GotoTerminator(loops.back().breakTarget);
}

void MIRBuilder::lowerContinue(HIRContinueStmt *) {
  getBlock(currentBlock)->terminator =
      GotoTerminator(loops.back().continueTarget);
}

void MIRBuilder::loewrLocalDecl(HIRLocalDeclStmt *stmt) {
  auto type = stmt->local->type->typeSymbol;
  if (type == nullptr) {
    Error::internal(stmt->span, "local decl's type is nullptr");
  }
  unique_ptr<MIRValue> init = nullptr;
  if (stmt->init) {
    init = lowerExpr(stmt->init.get());
  }
  emit(make_unique<MIRLocalDeclStmt>(type, stmt->local->symbol,
                                     std::move(init)));
}

void MIRBuilder::lowerReturn(HIRReturnStmt *stmt) {
  unique_ptr<MIRValue> v = nullptr;
  if (stmt->value) {
    v = lowerExpr(stmt->value.get());
  }
  getBlock(currentBlock)->terminator = ReturnTerminator(std::move(v));
}

void MIRBuilder::lowerSwitch(HIRSwitchStmt *stmt) {
  BlockID cond = currentBlock;
  BlockID defaultTarget = makeBlock();
  BlockID join = makeBlock();
  vector<MIRCase> cases;

  bool hasDefault = false;

  unique_ptr<MIRValue> condExpr = lowerExpr(stmt->cond.get());
  auto temp = makeTemp(stmt->cond->type->typeSymbol);

  emit(make_unique<MIRLocalDeclStmt>(temp->typeSymbol, temp,
                                     std::move(condExpr)));

  for (auto &c : stmt->cases) {
    assert(c->defaultKind != HIRDefaultKind::WildCard);
    BlockID id;
    if (c->defaultKind == HIRDefaultKind::Default) {
      id = defaultTarget;
      hasDefault = true;
    } else {
      id = makeBlock();
    }
    currentBlock = id;
    for (auto &v : c->selectors) {
      auto s = &v->selector;
      if (auto lit = std::get_if<HIRLiteralCase>(s)) {
        cases.push_back(MIRCase(lit->expr->resolvedLit, id));
        continue;
      }
      if (auto unit = std::get_if<HIRUnitCase>(s)) {
        cases.push_back(MIRCase(unit->variant->symbol, id));
        continue;
      }
      if (auto payload = std::get_if<HIRPayloadCase>(s)) {

        emit(make_unique<MIRLocalDeclStmt>(
            payload->binding->type->typeSymbol, payload->binding->symbol,
            make_unique<MIRPayloadExtractExpr>(
                payload->variant->symbol,
                make_unique<MIRLoad>(make_unique<MIRLocalPlace>(temp),
                                     temp->typeSymbol),
                temp->typeSymbol)));
        cases.push_back(MIRCase(payload->variant->symbol, id));
      }
    }
    lowerBlock(c->body.get());
    if (!hasTerminator(currentBlock)) {
      getBlock(currentBlock)->terminator = GotoTerminator(join);
    }
  }

  if (hasDefault && !hasTerminator(defaultTarget)) {
    getBlock(defaultTarget)->terminator = GotoTerminator(join);
  }

  getBlock(cond)->terminator = SwitchTerminator(
      make_unique<MIRLoad>(make_unique<MIRLocalPlace>(temp), temp->typeSymbol),
      std::move(cases), hasDefault ? defaultTarget : join);
  currentBlock = join;
}

void MIRBuilder::lowerValueTransfer(HIRValueTransferStmt *stmt) {
  unique_ptr<MIRValue> value = lowerExpr(stmt->value.get());
  emit(make_unique<MIRAssignStmt>(
      make_unique<MIRLocalPlace>(matches.back().result), std::move(value)));
  getBlock(currentBlock)->terminator = GotoTerminator(matches.back().join);
}

void MIRBuilder::lowerDestroy(HIRDestroyStmt *stmt) {
  unique_ptr<MIRValue> handle = lowerExpr(stmt->handle.get());
  emit(make_unique<MIRDestroyStmt>(std::move(handle)));
}
