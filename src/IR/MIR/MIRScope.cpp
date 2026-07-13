#include "hrd/IR/IRScope.h"
#include "hrd/IR/MIR/MIRBuilder.h"
#include <memory>
#include <utility>

IRScope *MIRBuilder::enterScope() {
  unique_ptr<IRScope> scope = make_unique<IRScope>(
      currentScope, currentScope != nullptr ? currentScope->depth + 1 : 0);
  auto raw = scope.get();
  currentScope = raw;
  scopes.push_back(std::move(scope));
  return raw;
}

void MIRBuilder::exitScope() { currentScope = currentScope->parent; }

void MIRBuilder::emitCleanup(IRScope *scope) {
  if (scope == nullptr) {
    return;
  }
  emit(make_unique<MIRCleanupStmt>(scope->locals));
}

void MIRBuilder::emitCleanupUntil(IRScope *toExclusive) {
  for (IRScope *scope = currentScope; scope != toExclusive;
       scope = scope->parent) {
    if (scope == nullptr) {
      Error::internal("invalid cleanup scope chain");
    }
    emitCleanup(scope);
  }
}