#include "IR/HIR/HIRBuilder.h"
#include "AST/Program.h"
#include "AST/Stmt.h"
#include "IR/HIR/HIRExpr.h"
#include "IR/HIR/HIRProgram.h"
#include "IR/HIR/HIRStmt.h"
#include "util/Error.h"

#include <cassert>
#include <utility>

HIRBuilder::HIRBuilder(SymbolTable *t, HIRProgram *p, HIRSource *s)
    : program(p), source(s), table(t) {}

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
