#include "IR/MIR/MIRBuilder.h"
#include "IR/HIR/HIRDecl.h"
#include "IR/MIR/MIRNode.h"
#include "IR/MIR/MIRType.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "util/MIRGuard.h"
#include <memory>
#include <utility>

void MIRBuilder::build() {
  for (auto &s : HirProgram->sources) {
    for (auto &d : s->typeDecls) {
      auto type = make_unique<MIRType>();
      type->type = d->type->typeSymbol;
      auto raw = type.get();
      program->types.push_back(std::move(type));
      program->typeMap.emplace(d->type->typeSymbol, raw);
    }
  }

  for (auto &s : HirProgram->sources) {
    for (auto &d : s->typeDecls) {
      lowerType(d.get());
    }
  }
}

void MIRBuilder::lowerType(HIRTypeDecl *type) {

  for (auto &m : type->methods) {
    lowerMethod(type->symbol, m.get());
  }
}
void MIRBuilder::lowerMethod(TypeSymbol *owner, HIRMethodDecl *method) {
  auto func = std::make_unique<MIRFunction>(method->symbol, owner);
  auto *raw = func.get();

  FuncGuard _(currentFunc, raw);

  currentBlock = makeBlock();
  raw->entry = currentBlock;

  for (auto &p : method->params) {
  }

  lowerBlock(method->body.get());

  if (!hasTerminator(currentBlock)) {
    getBlock(currentBlock)->terminator = ReturnTerminator();
  }

  program->functions.push_back(std::move(func));
}
