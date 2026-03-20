#include "SemanticAnalyzer.h"
#include "AST/Stmt.h"
#include "SemanticAnalyzer/Builder.h"
#include "SemanticAnalyzer/Linker.h"

#include <stdexcept>
#include <vector>

using std::vector;

SemanticAnalyzer::SemanticAnalyzer(vector<Stmt::Ptr> s) { ast = std::move(s); }

void SemanticAnalyzer::build() {
  Builder builder(&symbolTable);

  try {
    for (auto a : ast) {
      a->accept(&builder);
    }
    builder.linkRoot();
  } catch (std::runtime_error &e) {
    throw e;
  }
}

void SemanticAnalyzer::link() {
  Linker linker(&symbolTable);
  try {
    for (auto a : ast) {
      a->accept(&linker);
    }
  } catch (std::runtime_error &e) {
    throw e;
  }
}

void SemanticAnalyzer::resolve() {
  Resolver resolver(&symbolTable);
  try {

    for (auto a : ast) {
      a->accept(&resolver);
    }
  } catch (std::runtime_error &e) {
    throw e;
  }
}
