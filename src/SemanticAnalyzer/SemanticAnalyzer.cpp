#include "SemanticAnalyzer.h"
#include "AST/Program.h"
#include "SemanticAnalyzer/Builder.h"
#include "SemanticAnalyzer/Linker.h"
#include "SemanticAnalyzer/SymbolTable.h"

#include <stdexcept>

SemanticAnalyzer::SemanticAnalyzer(Program *p) : program(p) {}

void SemanticAnalyzer::build() {
  Builder builder(&symbolTable);
  for (auto &s : program->sources) {

    try {
      for (auto &a : s->decls) {
        a->accept(&builder);
      }
    } catch (std::runtime_error &e) {
      throw e;
    }
  }

  builder.linkRoot();
}

void SemanticAnalyzer::link() {
  Linker linker(&symbolTable);
  for (auto &s : program->sources) {
    try {
      for (auto &a : s->decls) {
        a->accept(&linker);
      }
    } catch (std::runtime_error &e) {
      throw e;
    }
  }
}

void SemanticAnalyzer::resolve() {
  Resolver resolver(&symbolTable);
  for (auto &s : program->sources) {

    try {
      for (auto &a : s->decls) {
        a->accept(&resolver);
      }
    } catch (std::runtime_error &e) {
      throw e;
    }
  }
}
