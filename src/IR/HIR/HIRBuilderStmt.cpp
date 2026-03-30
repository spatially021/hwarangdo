
#include "IR/HIR/HIRBuilder.h"
#include "util/Guard.h"

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
