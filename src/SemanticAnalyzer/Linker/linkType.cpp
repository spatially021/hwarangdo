#include "hrd/AST/Decl.h"
#include "hrd/AST/Expr.h"
#include "hrd/AST/Stmt.h"
#include "hrd/SemanticAnalyzer/Linker.h"
#include "hrd/SemanticAnalyzer/Scope.h"
#include "hrd/SemanticAnalyzer/SymbolTable/SymbolTable.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/compiler/CompilerContexts.h"
#include "hrd/util/Error.h"
#include "hrd/util/Guard.h"
#include "hrd/util/Helper.h"
#include "hrd/util/TypeResolver.h"
#include <cassert>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

void Linker::visit(ClassDecl *decl) {

  if (decl->symbol->type == Symbol::SymbolType::MAIN) {
    auto symbol = static_cast<MainSymbol *>(decl->symbol);
    auto &bucket = symbol->methodMap["update"];
    if (bucket.size() == 0) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S009);
      dia.labels = {
          {decl->span, "the 'Main' class must declare an 'update' frame method",
           true},
      };
      engine.emit(dia);
      recover.recover();
    } else if (bucket.size() > 1) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S002);
      dia.labels = {
          {decl->span, "duplicate 'update' method", true},
          {table.getType(decl->name)->decl->span,
           "previous 'update' method declared here", false},
      };
      engine.emit(dia);
      recover.recover();
    }
    symbol->update = bucket[0];

    bucket = symbol->inits;
    if (bucket.size() > 1) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S010);
      dia.labels = {
          {decl->span, "duplicate 'Main' init method", true},
          {bucket[0]->decl->span, "previous 'Main' init method declared here",
           false},
      };
      engine.emit(dia);
      recover.recover();
    }

    if (bucket.size() == 1) {
      auto init = bucket[0];
      if (!init->params.empty()) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S011);
        dia.labels = {
            {decl->span, "'Main' init method must not have parameters", true},

        };
        engine.emit(dia);
        recover.recover();
      }
      symbol->init = init;
    }
  }

  if (decl->baseClass.has_value()) {
    decl->baseClass.value()->accept(this);
  }
  for (auto &t : decl->traits) {
    if (!table.isType(t.str)) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S013);
      dia.labels = {
          {t.span, "type '" + t.str + "' not found ", true},

      };
      engine.emit(dia);
      recover.recover();
    }
    auto symbol = table.getType(t.str);
    if (symbol->kind != TypeKind::TRAIT) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S014);
      dia.labels = {
          {t.span, "this type is not a trait", true},

      };
      engine.emit(dia);
      recover.recover();
    }
    if (!decl->symbol->traits.emplace(symbol).second) {
      auto prev = decl->symbol->traitSpan.find(symbol);
      if (prev == decl->symbol->traitSpan.end()) {
        Error::internal("fail to get trait span");
      }
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S015);
      dia.labels = {
          {t.span, "duplicate implementation of this trait", true},
          {prev->second, "previous implementation is here", false},
      };
      engine.emit(dia);
      recover.recover();
    }
    decl->symbol->traitSpan.emplace(symbol, t.span);
  }

  ScopeGuard _(table, decl->symbol->memberScope);
  TypeContextGuard __(currentType, decl->symbol);

  for (auto &a : decl->fields) {
    a->accept(this);
  }
  for (auto &a : decl->methods) {
    a->accept(this);
  }
}

void Linker::visit(StructDecl *decl) {
  ScopeGuard _(table, decl->symbol->memberScope);
  for (auto &f : decl->fields) {
    f->accept(this);
  }

  for (auto &i : decl->inits) {
    i->accept(this);
  }
}

void Linker::visit(EnumDecl *decl) {
  ScopeGuard _(table, decl->symbol->scope);
  for (auto &v : decl->variants) {
    if (v->payload.has_value()) {
      auto t = v->payload.value().get();
      TypeSymbol *s = nullptr;
      {
        auto &map = decl->symbol->getGenericParamMap();
        auto it = map.find(t->type);
        if (it == map.end()) {
          s = table.getType(t);
        } else {
          s = it->second;
        }
      }
      if (s == nullptr) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S013);
        dia.labels = {
            {v->token.span, "type '" + t->type + "' not found ", true},
        };
        engine.emit(dia);
        recover.recover();
      }

      if (s->kind == TypeKind::CLASS) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S016);
        dia.labels = {
            {decl->span, "entity type used here", true},
        };
        dia.notes = {{"enum variant payloads cannot contain entity types"}};
        engine.emit(dia);
        recover.recover();
      }
      t->resolved = s;
      v->symbol->payloadType = s;
    }
  }
}

void Linker::visit(ImplDecl *decl) {
  ScopeGuard _(table, decl->symbol->scope);
  auto impl = decl->symbol;
  auto s = decl->target;
  Token &name = s.name;
  if (!table.isType(name.text)) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S013);
    dia.labels = {
        {name.span, "type '" + s.name.text + "' not found ", true},

    };
    engine.emit(dia);
    recover.recover();
  }
  auto symbol = table.getType(s.name.text);
  if (symbol->kind != TypeKind::STRUCT) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S017);
    dia.labels = {
        {decl->span, "'" + name.text + "' is not a struct type", true},
    };
    engine.emit(dia);
    recover.recover();
  }
  auto obj = dyn_cast<ObjectType>(symbol);
  decl->importTarget = obj;
  {
    auto &params = obj->getGenericParams();
    if (params.size() == decl->target.genericParams.size()) {
      auto &map = impl->getGenericParamMap();
      for (size_t i = 0; i < params.size(); ++i) {
        auto n = decl->target.genericParams[i].name.text;
        auto [__, inserted] = map.emplace(n, params[i]);
        table.scopeManger.addGenericParam(n, params[i]);
        if (!inserted) {
          auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S137);
          dia.labels = {
              {decl->target.genericParams[i].name.span,
               "this generic parameter name is already declared", true},
          };
          dia.notes = {
              "generic parameter names must be unique within the same "
              "declaration",
          };
          dia.helps = {
              "rename one of the duplicate generic parameters",
          };
          engine.emit(dia);
          recover.recover();
        }
      }
    } else {
      if (decl->target.genericParams.empty() && !params.empty()) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S139);
        dia.labels = {
            {decl->span, "this impl does not declare generic parameters", true},
            {obj->decl->span, "this target type is generic", false},
        };
        dia.notes = {
            "an impl must match the generic structure of its target type",
        };
        dia.helps = {
            "declare the generic parameters required by the target type",
        };
        engine.emit(dia);
        recover.recover();
      } else if (!decl->target.genericParams.empty() && params.empty()) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S138);
        dia.labels = {
            {decl->span, "this impl declares generic parameters", true},
            {obj->decl->span, "this target type is not generic", false},
        };
        dia.notes = {
            "an impl must match the generic structure of its target type",
        };
        dia.helps = {
            "remove the generic parameters from this impl",
        };
        engine.emit(dia);
        recover.recover();
      } else {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S140);
        dia.labels = {
            {decl->span,
             "this impl provides " +
                 std::to_string(decl->target.genericParams.size()) +
                 " generic arguments",
             true},
            {obj->decl->span,
             "this type requires " + std::to_string(params.size()) +
                 " generic arguments",
             false},
        };
        dia.notes = {
            "the number of generic arguments in an impl must match the target "
            "type's generic parameters",
        };
        dia.helps = {
            "provide exactly " + std::to_string(params.size()) +
                " generic arguments for this impl target",
        };
        engine.emit(dia);
        recover.recover();
      }
    }
  }

  for (auto &c : decl->traits) {
    auto t = table.getType(c.str);
    if (t->kind != TypeKind::TRAIT) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S014);
      dia.labels = {
          {c.span, "this type is not a trait", true},

      };
      engine.emit(dia);
      recover.recover();
    }
    if (!decl->traitSpan.emplace(t, c.span).second) {
      auto prev = decl->traitSpan.find(t);
      if (prev == decl->traitSpan.end()) {
        Error::internal("fail to get trait span");
      }
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S015);
      dia.labels = {
          {c.span, "duplicate implementation of this trait", true},
          {prev->second, "previous implementation is here", false},
      };
      engine.emit(dia);
      recover.recover();
    }

    for (auto &it : t->methodMap) {
      for (auto sig : it.second) {
        decl->sigs.push_back(sig);
      }
    }
  }

  TypeContextGuard __(currentType, symbol);

  impl->target = obj;
  if (impl->target == nullptr) {
    Error::internal(decl->span, "impl target is nullptr");
  }
  if (impl->target->memberScope == nullptr) {
    Error::internal(decl->span, "impl target's memberScope is nullptr");
  }

  for (auto &a : decl->LinkedImplMethods) {
    MethodSymbol *methodSymbol = a->methodSymbol;
    if (methodSymbol == nullptr) {
      Error::internal(a->span, "not built methodSymbol : " + a->name);
    }
    if (auto [result, span] =
            obj->addMethod(methodSymbol, methodSymbol->isStatic);
        !result) {
      auto it = obj->methodMap.find(a->name);
      if (it == obj->methodMap.end()) {
        Error::internal("fail to get method symbol");
      }
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S018);
      dia.labels = {
          {a->span, "duplicate impl method declared here", true},
          {span, "previous impl method declared here", false},
      };
      dia.notes = {
          "method signatures must be unique within the same type",
      };
      engine.emit(dia);
      recover.recover();
    }

    a->accept(this);

    methodSymbol->owner = symbol;
    methodSymbol->selfScope = obj->memberScope;
  }
}

void Linker::visit(TraitDecl *decl) {
  for (auto &s : decl->traitSigs) {
    s->accept(this);
  }
}

void Linker::visit(TypeNode *type) {
  TypeResolverContext context = {engine, table, recover};
  TypeResolver::resolveTypeNode(type, context);
}
