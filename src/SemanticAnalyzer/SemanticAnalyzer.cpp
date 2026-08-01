#include "hrd/SemanticAnalyzer.h"
#include "hrd/AST/Program.h"
#include "hrd/Recover/SementicRecover.h"
#include "hrd/SemanticAnalyzer/Builder.h"
#include "hrd/SemanticAnalyzer/Linker.h"
#include "hrd/SemanticAnalyzer/SymbolTable.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/compiler/CompilerContexts.h"

#include <stdexcept>

SemanticAnalyzer::SemanticAnalyzer(SemanContext &context)
    : program(context.program), symbolTable(context.table),
      engine(context.engine), recover(*this) {}

void SemanticAnalyzer::build() {
  BuilderContext context = {symbolTable, engine};
  Builder builder(context);
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
  LinkerContext context = {symbolTable, engine};
  Linker linker(context);
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
  ResolverContext context = {symbolTable, engine};
  Resolver resolver(context);
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
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S123);
    dia.labels = {
        {type->decl->span, "cyclic type layout dependency detected here", true},
    };
    dia.notes = {
        "field layout cannot be calculated for cyclic inheritance",
    };
    dia.helps = {
        "remove the cyclic inheritance relationship",
    };
    engine.emit(dia);
    recover.recover();
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