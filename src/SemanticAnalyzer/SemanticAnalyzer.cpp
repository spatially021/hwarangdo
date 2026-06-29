#include "hrd/SemanticAnalyzer.h"
#include "hrd/AST/Program.h"
#include "hrd/SemanticAnalyzer/Builder.h"
#include "hrd/SemanticAnalyzer/Linker.h"
#include "hrd/SemanticAnalyzer/SymbolTable.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"

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

  for (auto &t : symbolTable.types) {
    fieldIndexing(t);
  }

  prepareRuntime();
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

void SemanticAnalyzer::fieldIndexing(TypeSymbol *type) {
  if (type->kind != TypeSymbol::TypeKind::CLASS &&
      type->kind != TypeSymbol::TypeKind::STRUCT) {
    return;
  }

  if (layoutState[type] == LayoutState::Done) {
    return;
  }

  if (layoutState[type] == LayoutState::Visiting) {
    Error::diagnostic(type->decl->span, "cyclic inheritance detected");
    return;
  }

  layoutState[type] = LayoutState::Visiting;

  uint32_t index = 0;

  if (type->base != nullptr) {
    fieldIndexing(type->base);

    if (layoutState[type->base] != LayoutState::Done) {
      return;
    }

    index = type->base->fieldCount;
  }

  for (auto *field : type->fields) {
    field->index = index++;
  }

  type->fieldCount = index;
  layoutState[type] = LayoutState::Done;
}