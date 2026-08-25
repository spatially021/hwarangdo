#include "hrd/IR/HIR/HIRBuilder.h"
#include "hrd/AST/Program.h"
#include "hrd/AST/Stmt.h"
#include "hrd/IR/HIR/HIRExpr.h"
#include "hrd/IR/HIR/HIRProgram.h"
#include "hrd/IR/HIR/HIRStmt.h"
#include "hrd/Recover/HIRReover.h"
#include "hrd/compiler/CompilerContexts.h"
#include "hrd/util/Error.h"

#include <cassert>
#include <utility>

HIRBuilder::HIRBuilder(HIRContext &ctx, HIRSource *s)
    : program(ctx.program), source(s), engine(ctx.engine), recover(*this),
      table(ctx.table) {}

void HIRBuilder::build() {

  for (auto &d : source->source->decls) {
    d->accept(this);
  }
}

void HIRBuilder::emit(unique_ptr<HIRStmt> stmt) {
  if (currentBlock == nullptr) {
    Error::internal("currentBlock is nullptr");
  }
  if (stmt == nullptr) {
    Error::internal("emit stmt is nullptr");
  }
  currentBlock->statements.push_back(std::move(stmt));
}
