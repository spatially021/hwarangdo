
#include "AST/Expr.h"
#include "AST/Stmt.h"
#include "IR/HIR/HIRBuilder.h"
#include "IR/HIR/HIRStmt.h"
#include "util/Guard.h"
#include <memory>
#include <utility>

unique_ptr<HIRBlockStmt> HIRBuilder::lowerBlock(BlockStmt *stmt) {

  unique_ptr<HIRBlockStmt> block = make_unique<HIRBlockStmt>();
  BlockGuard _(currentBlock, block.get());
  for (auto &s : stmt->statements) {
    s->accept(this);
  }
  return block;
}

unique_ptr<HIRBlockStmt> HIRBuilder::lowerStmtAsBlock(Stmt *stmt) {
  if (auto b = dynamic_cast<BlockStmt *>(stmt)) {
    return lowerBlock(b);
  }

  unique_ptr<HIRBlockStmt> block = make_unique<HIRBlockStmt>();
  BlockGuard _(currentBlock, block.get());
  stmt->accept(this);
  return block;
}

unique_ptr<HIRStmt> HIRBuilder::lowerFor(ForStmt *stmt) {
  HIRLocal *local = nullptr;

  if (auto d = dynamic_cast<DeclStmt *>(stmt->initializer.get())) {
    if (auto v = dynamic_cast<VarDecl *>(d->decl.get())) {
      local = lowerLocal(v);
    } else {
      Error::internal(stmt->token, "for initializer decl is not VarDecl");
    }
  } else {
    Error::internal(stmt->token, "for initializer is not DeclStmt");
  }

  auto from = lowerExpr(stmt->range->from.get());
  auto to = lowerExpr(stmt->range->to.get());
  auto step = lowerExpr(stmt->range->step.get());
  auto body = lowerStmtAsBlock(stmt->body.get());

  return make_unique<HIRForRangeStmt>(local, std::move(from), std::move(to),
                                      std::move(step), std::move(body));
}
unique_ptr<HIRStmt> HIRBuilder::lowerIf(IfStmt *stmt) {
  unique_ptr<HIRExpr> cond = lowerExpr(stmt->condition.get());
  unique_ptr<HIRBlockStmt> thenBlock = lowerStmtAsBlock(stmt->thenBranch.get());
  unique_ptr<HIRBlockStmt> elseBlock =
      stmt->elseBranch ? lowerStmtAsBlock(stmt->elseBranch.get()) : nullptr;
  return make_unique<HIRIfStmt>(std::move(cond), std::move(thenBlock),
                                std::move(elseBlock));
}

unique_ptr<HIRCase> HIRBuilder::lowerCase(Case *stmt) {

  vector<unique_ptr<HIRValueExpr>> selectors;
  for (auto &s : stmt->values) {
    selectors.push_back(lowerValue(s.get()));
  }
  unique_ptr<HIRBlockStmt> body = lowerStmtAsBlock(stmt->body.get());
  return make_unique<HIRCase>(std::move(selectors), std::move(body),
                              stmt->isDefault);
}

unique_ptr<HIRStmt> HIRBuilder::lowerWhile(WhileStmt *stmt) {
  unique_ptr<HIRExpr> cond = lowerExpr(stmt->condition.get());
  unique_ptr<HIRBlockStmt> body = lowerStmtAsBlock(stmt->body.get());
  return (make_unique<HIRWhileStmt>(std::move(cond), std::move(body)));
}

unique_ptr<HIRStmt> HIRBuilder::lowerReturn(ReturnStmt *stmt) {
  std::unique_ptr<HIRExpr> expr = nullptr;
  if (stmt->value) {
    expr = lowerExpr(stmt->value.get());
  }
  return (make_unique<HIRReturnStmt>(std::move(expr)));
}

unique_ptr<HIRStmt> HIRBuilder::lowerSwitch(SwitchStmt *stmt) {
  unique_ptr<HIRValueExpr> cond = lowerValue(stmt->value.get());
  vector<unique_ptr<HIRCase>> cases;
  for (auto &c : stmt->clauses) {
    cases.push_back(lowerCase(c.get()));
  }
  return (make_unique<HIRSwitchStmt>(std::move(cond), std::move(cases)));
}

unique_ptr<HIRStmt> HIRBuilder::lowerValueTransfer(ValueTransferStmt *stmt) {
  unique_ptr<HIRValueExpr> value = lowerValue(stmt->value.get());
  return make_unique<HIRValueTransferStmt>(std::move(value));
}

unique_ptr<HIRStmt> HIRBuilder::lowerExprStmt(ExprStmt *stmt) {

  if (auto destroy = dynamic_cast<DestroyExpr *>(stmt->expr.get())) {
    return lowerDestroyStmt(destroy);
  } else {
    return make_unique<HIRExprStmt>(lowerExpr(stmt->expr.get()));
  }
}

unique_ptr<HIRStmt> HIRBuilder::lowerDestroyStmt(DestroyExpr *expr) {
  auto storage = dynamic_cast<BuiltInNameExpr *>(expr->storage.get());
  if (storage == nullptr) {
    Error::internal(expr->token, "iliegal astNode kind");
  }
  StorageKind storageKind;
  switch (storage->storageType) {
  case BuiltInNameExpr::StorageType::WORLD:
    storageKind = StorageKind::World;
    break;
  case BuiltInNameExpr::StorageType::ARENA:
    storageKind = StorageKind::Arena;
    break;
  default:
    Error::internal(expr->token, "unknown storage kind");
  };

  NameExpr *name = dynamic_cast<NameExpr *>(expr->target.get());

  if (name == nullptr) {
    Error::internal(expr->token,
                    "illegal astNode type : " + expr->target->token.text);
  }

  auto place = lowerPlace(name);

  auto handleType = dynamic_cast<HIRHandleType *>(place->type);
  if (handleType == nullptr) {
    Error::internal(expr->token, "expect handle : " + place->type->name);
  }

  if (handleType->storage != storageKind) {
    Error::internal(expr->token, "handle storage kind mismatch");
  }

  HIREntityType *entity = handleType->entityType;

  unique_ptr<HIRValueExpr> handle = load(std::move(place));

  return make_unique<HIRDestroyStmt>(std::move(handle), entity, storageKind);
}