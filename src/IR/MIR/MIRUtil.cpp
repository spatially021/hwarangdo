#include "IR/MIR/MIRBuilder.h"
#include "IR/MIR/MIRNode.h"
#include "IR/MIR/MIRStmt.h"
#include "SemanticAnalyzer/SymbolTable.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "SemanticAnalyzer/symbol/ValueSymbol.h"
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
  table->addTemp(std::move(symbol));
  return raw;
}
