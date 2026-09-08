#include "hrd/SemanticAnalyzer.h"
#include "hrd/AST/Decl.h"
#include "hrd/AST/Program.h"
#include "hrd/Recover/SementicRecover.h"
#include "hrd/SemanticAnalyzer/Builder.h"
#include "hrd/SemanticAnalyzer/Linker.h"
#include "hrd/SemanticAnalyzer/Module.h"
#include "hrd/SemanticAnalyzer/SymbolTable/SymbolTable.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/compiler/CompilerContexts.h"
#include "hrd/diagnostic/Diagnostic.h"

#include <cassert>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

SemanticAnalyzer::SemanticAnalyzer(SemanContext &context)
    : program(context.program), table(context.table), engine(context.engine),
      isCompile(context.isCompile), recover(*this) {}

void SemanticAnalyzer::build() {
  BuilderContext context = {table, engine};
  auto module = table.moudle;
  Builder builder(context);

  for (auto &s : program->sources) {
    try {
      auto file = make_unique<FileContext>(module, s->logicalPath);
      auto raw = file.get();
      table.registry.addFile(s->logicalPath, raw);
      s->fileContext = raw;
      module->files.push_back(std::move(file));
      table.registry.setCurrentFile(raw);
      for (auto &a : s->decls) {
        a->accept(&builder);
      }
    } catch (std::runtime_error &e) {
      throw e;
    }
  }

  builder.linkRoot(isCompile);
}

static std::string convertVecToString(vector<StringDatum> vec) {
  string str = "";
  for (unsigned i = 0; i < vec.size(); ++i) {
    str += vec[i].str;
    if (i != vec.size() - 1) {
      str += ".";
    }
  }
  return str;
}

void SemanticAnalyzer::import() {
  for (auto &source : program->sources) {
    auto *current = source->fileContext;

    for (auto &decl : source->decls) {
      auto *importDecl = dynamic_cast<ImportDecl *>(decl.get());
      if (importDecl == nullptr) {
        continue;
      }

      FileContext *file = nullptr;
      if (importDecl->module.has_value()) {
        auto m = table.registry.getModule(importDecl->module.value());
        if (m == nullptr) {
          Error::internal("fail to get module");
        }
        file = table.registry.getFile(m, importDecl->sPath);
      } else {
        file = table.registry.getFile(importDecl->sPath);
      }

      if (file == nullptr) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S126);
        dia.labels = {
            {decl->span, "this import path could not be resolved", true},
        };
        dia.labels = {
            {decl->span,
             "path '" + convertVecToString(importDecl->path) +
                 "' was not found in module '" +
                 ((importDecl->module.has_value())
                      ? (importDecl->module.value().str)
                      : (table.registry.getCurrentFile()->module->name)) +
                 "'",
             true},
        };
        engine.emit(dia);
        recover.recover();
      }

      if (importDecl->types.empty()) {
        const auto types = table.registry.findTypes(file);

        for (const auto &[name, symbol] : types) {
          if (table.registry.findType(current, name) != nullptr) {
            auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S125);
            dia.labels = {
                {importDecl->span,
                 "name `" + name + "` is already used by another type", true}};
            dia.notes = {{"imported types and aliases must have unique names "
                          "within a file"}};
            dia.helps = {{"use a different alias for this imported type"}};
          }
          auto [_, inserted] = current->importedTypes.emplace(name, symbol);
          if (!inserted) {
            auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S125);
            dia.labels = {
                {importDecl->span,
                 "name `" + name + "` is already used by another type", true}};
            dia.notes = {{"imported types and aliases must have unique names "
                          "within a file"}};
            dia.helps = {{"use a different alias for this imported type"}};
            engine.emit(dia);
            recover.recover();
          }
        }

        continue;
      }

      for (const auto &type : importDecl->types) {
        auto *symbol = table.registry.findType(file, type.origin.str);

        if (symbol == nullptr) {
          auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S124);
          dia.labels = {
              {type.origin.span,
               "type `" + type.origin.str + "` is not declared in this file",
               true}};
          dia.notes = {
              {"imports can only select types directly declared by the "
               "target file"}};
          dia.helps = {{"check the type name or import it from the file "
                        "where it is declared"}};
          engine.emit(dia);
          recover.recover();
        }

        if (table.registry.findType(current, type.origin.str) != nullptr) {
          auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S125);
          dia.labels = {
              {type.origin.span,
               "name `" + type.origin.str + "` is already used by another type",
               true}};
          dia.notes = {{"imported types and aliases must have unique names "
                        "within a file"}};
          dia.helps = {{"use a different alias for this imported type"}};
        }

        auto [_, inserted] =
            current->importedTypes.emplace(type.alias.str, symbol);

        if (!inserted) {
          auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S125);
          dia.labels = {
              {type.alias.span,
               "name `" + type.alias.str + "` is already used by another type",
               true}};
          dia.notes = {{"imported types and aliases must have unique names "
                        "within a file"}};
          dia.helps = {{"use a different alias for this imported type"}};
        }
      }
    }
  }
}

void SemanticAnalyzer::link() {

  LinkerContext context = {table, engine};
  Linker linker(context);
  for (auto &s : program->sources) {
    assert(s->fileContext);
    table.registry.setCurrentFile(s->fileContext);
    try {
      for (auto &a : s->decls) {
        a->accept(&linker);
      }
    } catch (std::runtime_error &e) {
      throw e;
    }
  }

  for (auto &t : table.registry.getTypes()) {
    fieldIndexing(t);
  }

  prepareRuntime();
}

void SemanticAnalyzer::resolve() {
  ResolverContext context = {table, engine};
  Resolver resolver(context);
  for (auto &s : program->sources) {
    assert(s->fileContext);
    table.registry.setCurrentFile(s->fileContext);
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
  if (type->kind != TypeKind::CLASS && type->kind != TypeKind::STRUCT) {
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
  if (auto obj = dyn_cast<ObjectType>(type)) {
    if (obj->base != nullptr) {
      fieldIndexing(obj->base);

      if (layoutState[obj->base] != LayoutState::Done) {
        return;
      }

      index = obj->base->fieldCount;
    }

    for (auto *field : obj->fields) {
      field->index = index++;
    }

    obj->fieldCount = index;
  }

  layoutState[type] = LayoutState::Done;
}