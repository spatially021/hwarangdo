#include "hrd/SemanticAnalyzer/Resolver.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/util/Error.h"
#include "hrd/util/diagnostic/Diagnostic.h"

void Resolver::visit(BuiltInNameExpr *expr) {
  expr->resolvedType = table.getBuiltName();

  switch (expr->token.kind) {
  case TKind::WORLD:
    expr->storageType = BuiltInNameExpr::StorageType::WORLD;
    break;

  default:
    Error::internal(expr->token,
                    "unmatched builtin token: " + expr->token.text);
  }
}

void Resolver::visit(SpawnExpr *expr) {
  expr->left->accept(this);

  if (expr->left->kind != NKind::BUILTIN_NAME_EXPR) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S104);
    dia.labels = {
        {expr->left->span, "this expression is not an entity storage", true},
    };
    dia.notes = {
        "entity creation requires a storage expression",
    };
    dia.helps = {
        "use 'world.spawn' to create an entity",
    };
    engine.emit(dia);
    recover.recover();
  }

  auto *storage = dynamic_cast<BuiltInNameExpr *>(expr->left.get());
  if (!storage) {
    Error::internal(expr->left->span,
                    "builtin storage expression has an invalid node type");
  }

  if (storage->storageType != BuiltInNameExpr::StorageType::WORLD) {
    Error::internal(expr->left->span, "unsupported entity storage type");
  }

  expr->spawnType->accept(this);

  if (!expr->spawnType->resolved) {
    Error::internal(expr->spawnType->span, "failed to resolve spawn type");
  }

  if (!expr->spawnType->resolved->memberScope) {
    Error::internal(expr->spawnType->span,
                    expr->spawnType->type + "'s member scope is nullptr");
  }

  if (expr->spawnType->resolved->kind != TypeSymbol::TypeKind::CLASS) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S105);
    dia.labels = {
        {expr->spawnType->span,
         "type '" + expr->spawnType->resolved->name + "' is not an entity type",
         true},
    };
    dia.notes = {
        "only class types can be created through 'world.spawn'",
    };
    dia.helps = {
        "use a class type as the spawn target",
    };
    engine.emit(dia);
    recover.recover();
  }

  vector<TypeSymbol *> args;
  args.reserve(expr->args.size());

  for (auto &arg : expr->args) {
    arg->accept(this);

    if (!arg->resolvedType) {
      Error::internal(arg->span, "failed to resolve spawn argument type");
    }

    args.push_back(arg->resolvedType);
  }

  auto [result, method] =
      lookupInit(expr->spawnType->resolved->memberScope, args);

  if (!result) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S021);
    dia.labels = {
        {expr->span,
         "no init of '" + expr->spawnType->resolved->name +
             "' accepts these arguments",
         true},
    };
    dia.notes = {
        "the argument count and types must match one of the declared init "
        "methods",
    };
    dia.helps = {
        "change the arguments to match an available init method",
    };
    engine.emit(dia);
    recover.recover();
  }

  vector<TypeSymbol *> typeArgs = {
      expr->spawnType->resolved,
  };

  expr->resolvedType = table.GenericInsGetOrCreate(table.getHandle(), typeArgs);
  expr->resolvedInit = method;
}

void Resolver::visit(ViewExpr *expr) {
  expr->left->accept(this);

  if (!expr->left->resolvedType) {
    Error::internal(expr->left->span, "failed to resolve view storage type");
  }

  if (expr->left->resolvedType != table.getBuiltName()) {
    Error::internal(expr->left->span,
                    "view storage has an unexpected resolved type");
  }

  auto *storage = dynamic_cast<BuiltInNameExpr *>(expr->left.get());
  if (!storage) {
    Error::internal(expr->left->span,
                    "view storage is not a builtin name expression");
  }

  if (storage->storageType != BuiltInNameExpr::StorageType::WORLD) {
    Error::internal(expr->left->span, "unsupported view storage type");
  }

  expr->target->accept(this);

  auto *handle = dynamic_cast<GenericSymbol *>(expr->target->resolvedType);

  if (!handle || handle->origin != table.getHandle()) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S106);
    dia.labels = {
        {expr->target->span, "this expression does not have a handle type",
         true},
    };
    dia.notes = {
        "'world.view' requires a handle that identifies an entity",
    };
    dia.helps = {
        "pass a Handle<T> value to 'world.view'",
    };
    engine.emit(dia);
    recover.recover();
  }

  if (!canPlaceView(expr->target)) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S107);
    dia.labels = {
        {expr->target->span,
         "this handle expression cannot be used to create an observer", true},
    };
    dia.notes = {
        "'world.view' requires a handle expression with a valid storage "
        "location",
    };
    dia.helps = {
        "store the handle in a variable before passing it to 'world.view'",
    };
    engine.emit(dia);
    recover.recover();
  }

  if (handle->args.empty()) {
    Error::internal(expr->target->span,
                    "handle type has no entity type argument");
  }

  expr->resolvedType = handle->args[0];
}

void Resolver::visit(DestroyExpr *expr) {
  expr->storage->accept(this);

  if (!expr->storage->resolvedType) {
    Error::internal(expr->storage->span,
                    "failed to resolve destroy storage type");
  }

  if (expr->storage->resolvedType != table.getBuiltName()) {
    Error::internal(expr->storage->span,
                    "destroy storage has an unexpected resolved type");
  }

  auto *storage = dynamic_cast<BuiltInNameExpr *>(expr->storage.get());
  if (!storage) {
    Error::internal(expr->storage->span,
                    "destroy storage is not a builtin name expression");
  }

  if (storage->storageType != BuiltInNameExpr::StorageType::WORLD) {
    Error::internal(expr->storage->span, "unsupported destroy storage type");
  }

  expr->target->accept(this);

  auto *handle = dynamic_cast<GenericSymbol *>(expr->target->resolvedType);

  if (!handle || handle->origin != table.getHandle()) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S106);
    dia.labels = {
        {expr->target->span, "this expression does not have a handle type",
         true},
    };
    dia.notes = {
        "'world.destroy' requires a handle that identifies an entity",
    };
    dia.helps = {
        "pass a Handle<T> value to 'world.destroy'",
    };
    engine.emit(dia);
    recover.recover();
  }

  if (!canPlaceView(expr->target)) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S107);
    dia.labels = {
        {expr->target->span,
         "this handle expression cannot be used as a destroy target", true},
    };
    dia.notes = {
        "'world.destroy' requires a handle expression with a valid storage "
        "location",
    };
    dia.helps = {
        "store the handle in a variable before passing it to 'world.destroy'",
    };
    engine.emit(dia);
    recover.recover();
  }

  expr->resolvedType = table.getType("void");
}

void Resolver::visit(QuitExpr *expr) {
  for (Scope *scope = table.getCurrent(); scope != nullptr;
       scope = scope->parent) {
    if (scope->scopeKind == Scope::ScopeKind::INIT) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S108);
      dia.labels = {
          {expr->span, "quit expression appears inside an init method", true},
      };
      dia.notes = {
          "an init method must complete object initialization normally",
      };
      dia.helps = {
          "move this quit expression outside the init method",
      };
      engine.emit(dia);
      recover.recover();
    }
  }
}