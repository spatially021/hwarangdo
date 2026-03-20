#include "IR/HIR/HIRBuilder.h"
#include "AST/Expr.h"
#include "AST/Stmt.h"
#include "IR/HIR/HIRExpr.h"
#include "IR/HIR/HIRStmt.h"
#include "util/Error.h"
#include "util/Guard.h"
#include <memory>
#include <utility>

void HIRBuilder::emit(unique_ptr<HIRStmt> stmt) {
  if (currentBlock == nullptr) {
    Error::internal("currentBlock is nullptr");
  }
  if (stmt == nullptr) {
    Error::internal("emit stmt is nullptr");
  }
  currentBlock->statements.push_back(std::move(stmt));
}

unique_ptr<HIRExpr> HIRBuilder::lowerExpr(Expr *expr) {
  if (expr == nullptr) {
    Error::internal("lowering expr is nullptr");
  }
  exprResult.reset();
  expr->accept(this);
  if (exprResult == nullptr) {
    Error::internal("expr result nullptr");
  }
  return std::move(exprResult);
}

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
