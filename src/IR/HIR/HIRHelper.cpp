
#include "hrd/IR/HIR/HIRHelper.h"
#include "hrd/AST/Decl.h"
#include "hrd/IR/HIR/HIRDecl.h"
#include "hrd/IR/HIR/HIRProgram.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/util/Error.h"
#include <memory>

void HIRHelper::linkSecondPass(HIRProgram *program) {
  for (auto &s : program->sources) {
    for (auto &d : s->source->decls) {
      if (auto *c = dynamic_cast<ClassDecl *>(d.get())) {
        auto it = program->typeDeclMap.find(c->symbol);
        if (it == program->typeDeclMap.end()) {
          Error::internal(d->span, "fail to get type");
        }
        if (c->baseClass.has_value()) {
          auto bIt = program->typeDeclMap.find(c->symbol->base);
          if (bIt == program->typeDeclMap.end()) {
            Error::internal(c->span, "fail to get baseType");
          }
          it->second->base = bIt->second;
        }
      }
    }
  }
}