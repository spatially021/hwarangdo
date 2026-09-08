#include "hrd/IR/MIR/MIRBuilder.h"
#include "hrd/IR/HIR/HIRDecl.h"
#include "hrd/IR/MIR/MIRNode.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/util/MIRGuard.h"
#include <memory>
#include <utility>

void MIRBuilder::build() {
  for (auto &s : HirProgram->sources) {
    for (auto &d : s->typeDecls) {
      if (d->type->kind == TypeKind::ENUM) {
        continue;
      }
      lowerType(d.get());
    }
  }
}

void MIRBuilder::lowerType(HIRTypeDecl *type) {

  unique_ptr<MIRFunction> defaultint =
      make_unique<MIRFunction>(nullptr, type->type);
  auto raw = defaultint.get();
  defaultint->isDefaultInit = true;
  program->functions.push_back(std::move(defaultint));
  currentFunc = raw;

  BlockID defaultInitBlock = makeBlock();
  currentBlock = defaultInitBlock;
  lowerBlock(type->defaultInitBlock.get());
  if (!hasTerminator(currentBlock)) {
    getBlock(currentBlock)->terminator = ReturnTerminator(nullptr);
  }

  for (auto &m : type->methods) {
    lowerMethod(type->symbol, m.get());
  }
}
void MIRBuilder::lowerMethod(TypeSymbol *owner, HIRMethodDecl *method) {
  auto func = std::make_unique<MIRFunction>(method->symbol, owner);
  auto *raw = func.get();
  FuncGuard _(currentFunc, raw);

  if (!method->symbol->isExtern) {
    currentBlock = makeBlock();
    raw->entry = currentBlock;
    enterScope();
    lowerBlock(method->body.get());
    exitScope();
    if (!hasTerminator(currentBlock)) {
      getBlock(currentBlock)->terminator = ReturnTerminator(nullptr);
    }
  }

  program->functions.push_back(std::move(func));
}
