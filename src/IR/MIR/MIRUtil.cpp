#include "hrd/IR/HIR/HIRExpr.h"
#include "hrd/IR/HIR/HIRStmt.h"
#include "hrd/IR/MIR/MIRBuilder.h"
#include "hrd/IR/MIR/MIRNode.h"
#include "hrd/IR/MIR/MIRStmt.h"
#include "hrd/SemanticAnalyzer/SymbolTable/SymbolTable.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include <memory>
#include <string>
#include <utility>
#include <variant>

BlockID MIRBuilder::makeBlock() { return currentFunc->createBlock(); }

BasicBlock *MIRBuilder::getBlock(BlockID id) {
  return currentFunc->getBlock(id);
}

bool MIRBuilder::hasTerminator(BlockID id) {
  return !std::holds_alternative<std::monostate>(getBlock(id)->terminator);
}

void MIRBuilder::emit(unique_ptr<MIRStmt> inst) {
  getBlock(currentBlock)->stmts.push_back(std::move(inst));
}

ValueSymbol *MIRBuilder::makeTemp(TypeSymbol *type) {
  unique_ptr<ValueSymbol> symbol = make_unique<ValueSymbol>();
  symbol->typeSymbol = type;
  symbol->name =
      "$tmp" + currentFunc->symbol->name + to_string(currentFunc->nextTemp++);
  auto raw = symbol.get();
  table.registry.addTemp(std::move(symbol));
  return raw;
}

void MIRBuilder::makeSwitch(SwitchData &data) {
  vector<MIRCase> cases;
  bool hasDefault = false;

  auto condExpr = lowerExpr(data.condExpr);
  auto temp = makeTemp(data.type);

  currentScope->locals.push_back(temp);
  emit(make_unique<MIRLocalDeclStmt>(temp->typeSymbol, temp,
                                     std::move(condExpr)));

  for (auto &c : data.cases) {

    BlockID id;
    if (c->defaultKind == HIRDefaultKind::Default ||
        c->defaultKind == HIRDefaultKind::WildCard) {
      id = data.defaultTarget;
      hasDefault = true;
    } else {
      id = makeBlock();
    }

    currentBlock = id;

    IRScope caseScope(&data.scope, data.scope.depth + 1);
    currentScope = &caseScope;

    for (auto &v : c->selectors) {
      auto &selector = v->selector;

      if (auto *lit = std::get_if<HIRLiteralCase>(&selector)) {
        cases.emplace_back(lit->expr->resolvedLit, id);
        continue;
      }

      if (auto *unit = std::get_if<HIRUnitCase>(&selector)) {
        cases.emplace_back(unit->variant, id);
        continue;
      }

      if (auto *payload = std::get_if<HIRPayloadCase>(&selector)) {
        auto *binding = payload->binding->symbol;

        caseScope.locals.push_back(binding);

        emit(make_unique<MIRLocalDeclStmt>(
            payload->binding->type, binding,
            make_unique<MIRPayloadExtractExpr>(
                payload->variant,
                make_unique<MIRLoad>(make_unique<MIRLocalPlace>(temp),
                                     temp->typeSymbol),
                temp->typeSymbol)));

        cases.emplace_back(payload->variant, id);
      }
    }

    lowerBlock(c->body.get());
    emitCleanup(&caseScope);

    if (!hasTerminator(currentBlock)) {
      getBlock(currentBlock)->terminator = GotoTerminator(data.cleanup);
    }

    currentScope = &data.scope;
  }

  if (hasDefault && !hasTerminator(data.defaultTarget)) {
    getBlock(data.defaultTarget)->terminator = GotoTerminator(data.cleanup);
  }

  getBlock(data.cond)->terminator = SwitchTerminator(
      make_unique<MIRLoad>(make_unique<MIRLocalPlace>(temp), temp->typeSymbol),
      std::move(cases), hasDefault ? data.defaultTarget : data.cleanup);

  currentBlock = data.cleanup;
  emitCleanup(&data.scope);
  getBlock(currentBlock)->terminator = GotoTerminator(data.join);
}