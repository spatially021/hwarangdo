

#include "IR/MIR/MIRBuilder.h"
#include "IR/MIR/MIRInst.h"
#include "IR/MIR/MIRNode.h"
#include <utility>
#include <variant>

BlockID MIRBuilder::makeBlock() { return currentFunc->createBlock(); }

BasicBlock *MIRBuilder::getBlock(BlockID id) {
  return currentFunc->getBlock(id);
}

bool MIRBuilder::hasTerminator(BlockID id) {
  return !std::holds_alternative<std::monostate>(getBlock(id)->terminator);
}

void MIRBuilder::emit(unique_ptr<MIRInst> inst) {
  getBlock(currentBlock)->insts.push_back(std::move(inst));
}