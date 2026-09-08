#include "hrd/SemanticAnalyzer/Builder.h"
#include "hrd/AST/ASTNode.h"
#include "hrd/AST/Decl.h"
#include "hrd/AST/DeclContext.h"
#include "hrd/AST/Expr.h"
#include "hrd/Inputs.h"
#include "hrd/Recover/BuilderRecover.h"
#include "hrd/SemanticAnalyzer/Scope.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/compiler/CompilerContexts.h"
#include "hrd/enums/MethodKind.h"
#include "hrd/util/Error.h"
#include "hrd/util/Guard.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

Builder::Builder(BuilderContext &ctx)
    : table(ctx.table), currentType(ctx.table.registry.getCurrent()),
      engine(ctx.engine), recover(*this) {
  topLevel = make_unique<TypeSymbol>();
  topLevel->name = "<top-level>";
  currentType = topLevel.get();
  rootScope->id = -1;
}

void Builder::visit(LiteralExpr *) {}
void Builder::visit(BinaryExpr *expr) {
  expr->left->accept(this);
  expr->right->accept(this);
}
void Builder::visit(NameExpr *) {}
void Builder::visit(UnaryExpr *expr) { expr->right->accept(this); }
void Builder::visit(CallExpr *expr) {
  if (expr->receiver != nullptr) {
    expr->receiver->accept(this);
  }
  for (auto a : expr->arguments) {
    a->accept(this);
  }
}
void Builder::visit(AssignExpr *expr) {
  expr->value->accept(this);
  expr->target->accept(this);
}
void Builder::visit(MemberExpr *expr) { expr->object->accept(this); }
void Builder::visit(ArrayAccessExpr *expr) {
  expr->object->accept(this);
  expr->index->accept(this);
}
void Builder::visit(TernaryExpr *expr) {
  expr->conditon->accept(this);
  expr->then->accept(this);
  if (expr->else_) {
    expr->else_->accept(this);
  }
}
void Builder::visit(ThisExpr *) {}
void Builder::visit(SuperExpr *) {}
void Builder::visit(RootExpr *) {};
void Builder::visit(SelfExpr *) {};
void Builder::visit(CastExpr *expr) {
  expr->left->accept(this);
  expr->type->accept(this);
}
void Builder::visit(BuiltInNameExpr *) {}
void Builder::visit(SpawnExpr *expr) {
  expr->left->accept(this);
  for (auto &a : expr->args) {
    a->accept(this);
  }
}
void Builder::visit(ViewExpr *expr) {
  expr->left->accept(this);
  expr->target->accept(this);
}
void Builder::visit(DestroyExpr *expr) {
  expr->storage->accept(this);
  expr->target->accept(this);
}
void Builder::visit(QuitExpr *) {}
void Builder::visit(DefaultValueExpr *) {}
void Builder::visit(Range *expr) {
  expr->from->accept(this);
  expr->to->accept(this);
}
void Builder::visit(CaseValueExpr *expr) {
  expr->value->accept(this);
  if (expr->arg) {
    expr->arg->accept(this);
  }
}
void Builder::visit(MatchExpr *expr) {
  ScopeGuard _(table);
  table.scopeManger.current()->scopeKind = Scope::ScopeKind::BLOCK;
  expr->blockScope = table.scopeManger.current();
  for (auto &c : expr->cases) {
    c->accept(this);
  }
}

void Builder::visit(ArrayLiteralExpr *expr) {
  for (auto &e : expr->elements) {
    e->accept(this);
  }
}

// Statement Builder::visitor methods
void Builder::visit(ExprStmt *stmt) { stmt->expr->accept(this); }
void Builder::visit(BlockStmt *stmt) {
  ScopeGuard _(table);
  stmt->blockScope = table.scopeManger.current();
  table.scopeManger.current()->scopeKind = Scope::ScopeKind::BLOCK;
  for (auto &s : stmt->statements) {
    s->accept(this);
  }
}
void Builder::visit(IfStmt *stmt) {
  stmt->thenBranch->accept(this);
  if (stmt->elseBranch != nullptr)
    stmt->elseBranch->accept(this);
}
void Builder::visit(ForStmt *stmt) {
  ScopeGuard _(table);
  stmt->blockScope = table.scopeManger.current();
  stmt->initializer->accept(this);
  table.scopeManger.current()->scopeKind = Scope::ScopeKind::BLOCK;
  stmt->body->accept(this);
}

void Builder::visit(WhileStmt *stmt) { stmt->body->accept(this); }

void Builder::visit(SwitchStmt *stmt) {
  ScopeGuard _(table);
  table.scopeManger.current()->scopeKind = Scope::ScopeKind::BLOCK;
  stmt->blockScope = table.scopeManger.current();

  for (auto &c : stmt->clauses) {
    c->accept(this);
  }
}
void Builder::visit(Case *stmt) { stmt->body->accept(this); }

void Builder::visit(ReturnStmt *stmt) {
  if (stmt->value) {
    stmt->value->accept(this);
  }
}
void Builder::visit(ValueTransferStmt *stmt) { stmt->value->accept(this); }
void Builder::visit(BreakStmt *) {}
void Builder::visit(ContinueStmt *) {}

void Builder::visit(DeclStmt *stmt) { stmt->decl->accept(this); }

void Builder::visit(EmptyStmt *) {}

void Builder::buildMain(ClassDecl *decl) {

  auto symbol = make_unique<MainSymbol>();
  symbol->name = decl->name;
  symbol->decl = decl;
  symbol->path = table.registry.getCurrentFile()->path;
  symbol->module = table.moudle;
  auto raw = symbol.get();

  auto result = table.add(std::move(symbol));

  if (!result.success) {
    switch (result.errorType) {
    case Result::DUPLICATED: {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S001);
      dia.labels = {
          {decl->span, "duplicate 'Main' class declared here", true},
          {table.main->decl->span, "previous 'Main' class declared here",
           false},
      };
      dia.notes = {{"only one 'Main' class may be declared in a program"}};
      engine.emit(dia);
      recover.recover();
    } break;

    case Result::RESERVED:
      Error::internal(decl->span, "unreachable");
      break;

    case Result::UNKNOWN_SYMBOL:
      Error::internal(decl->span, "unknown symbol '" + decl->name + "'");
      break;

    case Result::NONE:
      break;
    }
  }
  decl->symbol = raw;
  table.main = raw;

  TypeContextGuard _(currentType, raw);
  ScopeGuard __(table);

  raw->memberScope = table.scopeManger.current();
  table.scopeManger.current()->scopeKind = Scope::ScopeKind::FIELD;
  for (auto &a : decl->fields) {
    a->accept(this);
    raw->fields.push_back(a->symbol);
  }
  for (auto &a : decl->methods) {
    a->accept(this);
  }
}

void Builder::visit(ClassDecl *decl) {
  if (decl->name == "Main") {
    buildMain(decl);
    return;
  }

  auto symbol = make_unique<ObjectType>(TypeKind::CLASS);
  symbol->name = decl->name;
  symbol->decl = decl;
  symbol->baseName =
      decl->baseClass.has_value()
          ? std::optional<std::string>(decl->baseClass.value().str)
          : nullopt;
  symbol->path = table.registry.getCurrentFile()->path;
  symbol->module = table.moudle;
  auto raw = symbol.get();

  auto result = table.add(std::move(symbol));

  if (!result.success) {
    switch (result.errorType) {
    case Result::DUPLICATED: {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S002);
      dia.labels = {
          {decl->span, "type '" + decl->name + "' is already declared", true},
          {result.span, "previous declaration of '" + decl->name + "' is here",
           false},
      };
      dia.notes = {
          "type names must be unique within the same scope",
      };
      engine.emit(dia);
      recover.recover();
      break;
    }

    case Result::RESERVED: {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S003);
      dia.labels = {
          {decl->span, "'" + decl->name + "'is a reserved identifier", true},
      };
      dia.notes = {
          {"reserved identifiers cannot be used in user declarations"}};
      engine.emit(dia);
      recover.recover();
      break;
    }
    case Result::UNKNOWN_SYMBOL:
      Error::internal(decl->span, "unknown symbol '" + decl->name + "'");
      break;

    case Result::NONE:
      break;
    }
  }

  decl->symbol = raw;

  TypeContextGuard _(currentType, raw);
  ScopeGuard __(table);
  table.scopeManger.current()->scopeKind = Scope::ScopeKind::FIELD;
  raw->memberScope = table.scopeManger.current();
  for (auto &a : decl->fields) {
    a->accept(this);
    raw->fields.push_back(a->symbol);
  }
  for (auto &a : decl->methods) {
    a->accept(this);
  }
}

void Builder::visit(StructDecl *decl) {
  auto symbol = make_unique<ObjectType>(TypeKind::STRUCT);
  symbol->name = decl->name;
  symbol->decl = decl;
  symbol->path = table.registry.getCurrentFile()->path;
  symbol->module = table.moudle;
  auto raw = symbol.get();

  auto result = table.add(std::move(symbol));

  if (!result.success) {
    switch (result.errorType) {
    case Result::DUPLICATED: {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S002);
      dia.labels = {
          {decl->span, "type '" + decl->name + "' is already declared", true},
          {result.span, "previous declaration of '" + decl->name + "' is here",
           false},
      };
      dia.notes = {
          "type names must be unique within the same scope",
      };
      engine.emit(dia);
      recover.recover();
    } break;

    case Result::RESERVED: {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S003);
      dia.labels = {
          {decl->span, "'" + decl->name + "'is a reserved identifier", true},
      };
      dia.notes = {
          {"reserved identifiers cannot be used in user declarations"}};
      engine.emit(dia);
      recover.recover();
      break;
    } break;

    case Result::UNKNOWN_SYMBOL:
      Error::internal(decl->span, "unknown symbol '" + decl->name + "'");
      break;

    case Result::NONE:
      break;
    }
  }

  decl->symbol = raw;

  TypeContextGuard _(currentType, raw);
  ScopeGuard __(table);
  table.scopeManger.current()->scopeKind = Scope::ScopeKind::FIELD;
  raw->memberScope = table.scopeManger.current();
  for (auto &a : decl->fields) {
    a->accept(this);
    raw->fields.push_back(a->symbol);
  }

  for (auto &i : decl->inits) {
    i->accept(this);
  }
}

void Builder::visit(EnumDecl *decl) {

  auto symbol = make_unique<EnumType>();
  symbol->name = decl->name;
  symbol->decl = decl;
  symbol->path = table.registry.getCurrentFile()->path;
  symbol->module = table.moudle;

  auto raw = symbol.get();

  auto result = table.add(std::move(symbol));

  if (!result.success) {
    switch (result.errorType) {
    case Result::DUPLICATED: {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S002);
      dia.labels = {
          {decl->span, "type '" + decl->name + "' is already declared", true},
          {result.span, "previous declaration of '" + decl->name + "' is here",
           false},
      };
      dia.notes = {
          "type names must be unique within the same scope",
      };
      engine.emit(dia);
      recover.recover();
    } break;

    case Result::RESERVED: {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S003);
      dia.labels = {
          {decl->span, "'" + decl->name + "'is a reserved identifier", true},
      };
      dia.notes = {
          {"reserved identifiers cannot be used in user declarations"}};
      engine.emit(dia);
      recover.recover();
      break;
    } break;

    case Result::UNKNOWN_SYMBOL:
      Error::internal(decl->span, "unknown symbol '" + decl->name + "'");
      break;

    case Result::NONE:
      break;
    }
  }

  decl->symbol = raw;

  TypeContextGuard _(currentType, raw);

  uint32_t ordinal = 0;
  for (auto a : decl->variants) {
    auto v = make_unique<EnumVariantSymbol>();
    v->name = a->name;
    v->ordinal = ordinal++;
    if (raw->variantMap.count(v->name)) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S004);
      dia.labels = {
          {decl->span, "duplicate enum variant declared here", true},
          {result.span, "previous enum variant declared here", false},
      };
      dia.notes = {
          "an enum cannot contain multiple variants with the same name",
      };
      engine.emit(dia);
      recover.recover();
    }
    EnumVariantSymbol *r = v.get();
    v->typeSymbol = raw;
    raw->variants.push_back(std::move(v));
    raw->variantMap.emplace(r->name, r);
    a->symbol = r;
  }
}

void Builder::visit(ImplDecl *decl) {
  auto symbol = make_unique<ImplSymbol>();
  symbol->targetName = decl->target.str;
  symbol->decl = decl;

  auto raw = symbol.get();
  table.registry.addImpl(decl, std::move(symbol));

  TypeContextGuard __(currentType, raw);
  table.scopeManger.current()->scopeKind = Scope::ScopeKind::FIELD;

  for (auto &a : decl->LinkedImplMethods) {
    a->accept(this);
    a->methodSymbol->owner = nullptr;
  }
}

void Builder::visit(TraitDecl *decl) {
  auto symbol = make_unique<TraitType>();
  symbol->name = decl->name;
  symbol->decl = decl;
  auto raw = symbol.get();

  auto result = table.add(std::move(symbol));

  if (!result.success) {
    switch (result.errorType) {
    case Result::DUPLICATED: {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S002);
      dia.labels = {
          {decl->span, "type '" + decl->name + "' is already declared", true},
          {result.span, "previous declaration of '" + decl->name + "' is here",
           false},
      };
      dia.notes = {
          "type names must be unique within the same scope",
      };
      engine.emit(dia);
      recover.recover();
    } break;

    case Result::RESERVED: {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S003);
      dia.labels = {
          {decl->span, "'" + decl->name + "'is a reserved identifier", true},
      };
      dia.notes = {
          {"reserved identifiers cannot be used in user declarations"}};
      engine.emit(dia);
      recover.recover();
      break;
    } break;

    case Result::UNKNOWN_SYMBOL:
      Error::internal(decl->span, "unknown symbol '" + decl->name + "'");
      break;

    case Result::NONE:
      break;
    }
  }

  decl->symbol = raw;

  TypeContextGuard _(currentType, raw);
  table.scopeManger.current()->scopeKind = Scope::ScopeKind::FIELD;
  for (auto &a : decl->traitSigs) {
    if (a == nullptr)
      Error::internal("traitSig is nullptr");
    a->accept(this);
  }
}

void Builder::visit(TraitSig *sig) {

  auto symbol = make_unique<MethodSymbol>();

  symbol->name = sig->name;
  symbol->decl = sig;
  symbol->owner = currentType;
  auto raw = symbol.get();

  auto result = table.add(std::move(symbol));

  if (!result.success) {
    switch (result.errorType) {
    case Result::DUPLICATED: {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S007);
      dia.labels = {
          {sig->span, "duplicate method declared here", true},
          {result.span, "previous method declared here", false},
      };
      dia.notes = {
          "method signatures must be unique within the same type",
      };
      engine.emit(dia);
    } break;

    case Result::RESERVED: {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S003);
      dia.labels = {
          {sig->span, "'" + sig->name + "'is a reserved identifier", true},
      };
      dia.notes = {
          {"reserved identifiers cannot be used in user declarations"}};
      engine.emit(dia);
      recover.recover();
      break;
    } break;

    case Result::UNKNOWN_SYMBOL:
      Error::internal(sig->span, "unknown symbol '" + sig->name + "'");
      break;

    case Result::NONE:
      break;
    }
  }
  ScopeGuard _(table);
  table.scopeManger.current()->scopeKind = Scope::ScopeKind::FUNC;
  for (auto &a : sig->params) {
    auto s = make_unique<ParamSymbol>();
    s->name = a->name;
    s->kind = ValueSymbol::Kind::PARAM;
    s->node = a.get();
    s->nameSpan = a->span;
    auto r = s.get();

    auto re = table.add(std::move(s));

    if (!re.success) {
      switch (re.errorType) {
      case Result::DUPLICATED: {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S005);
        dia.labels = {
            {a->span, "duplicate variable declared here", true},
            {re.span, "previous variable declared here", false},
        };
        dia.notes = {
            "variable names must be unique within the same scope",
        };
        engine.emit(dia);
        break;
      }
      case Result::RESERVED: {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S003);
        dia.labels = {
            {sig->span, "'" + sig->name + "'is a reserved identifier", true},
        };
        dia.notes = {
            {"reserved identifiers cannot be used in user declarations"}};
        engine.emit(dia);
        recover.recover();
        break;
      } break;

      case Result::UNKNOWN_SYMBOL:
        Error::internal(a.get()->span,
                        "unknown symbol '" + a.get()->name + "'");
        break;

      case Result::NONE:
        break;
      }
    }
    a->symbol = r;
    a->type->accept(this);
  }

  sig->symbol = raw;
}

void Builder::visit(FuncDecl *decl) {
  auto symbol = make_unique<MethodSymbol>();

  symbol->name = decl->name;
  symbol->decl = decl;
  symbol->owner = currentType;
  symbol->declType = currentType;
  symbol->modifier = decl->aModifier;
  symbol->module = table.moudle;
  symbol->path = table.registry.getCurrentFile()->path;
  symbol->isExtern = decl->prefix.isExtern;
  if (symbol->isExtern) {
    if (decl->prefix.linkName.has_value()) {
      symbol->linkName = decl->prefix.linkName.value();
    } else {
      symbol->linkName = decl->name;
    }
  }
  symbol->isFrame = decl->prefix.isFrame;
  symbol->isOverride = decl->prefix.isOverride;
  symbol->methodKind = MethodKind::Normal;
  symbol->isStatic = decl->prefix.isStatic;

  auto raw = symbol.get();
  auto result = table.add(std::move(symbol));

  if (!result.success) {
    switch (result.errorType) {
    case Result::DUPLICATED: {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S007);
      dia.labels = {
          {decl->span, "duplicate method declared here", true},
          {result.span, "previous method declared here", false},
      };
      dia.notes = {
          "method signatures must be unique within the same type",
      };
      engine.emit(dia);
      recover.recover();
    } break;

    case Result::RESERVED: {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S003);
      dia.labels = {
          {decl->span, "'" + decl->name + "'is a reserved identifier", true},
      };
      dia.notes = {
          {"reserved identifiers cannot be used in user declarations"}};
      engine.emit(dia);
      recover.recover();
      break;
    } break;

    case Result::UNKNOWN_SYMBOL:
      Error::internal(decl->span, "unknown symbol '" + decl->name + "'");
      break;

    case Result::NONE:
      break;
    }
  }
  decl->methodSymbol = raw;

  ScopeGuard _(table);

  raw->scope = table.scopeManger.current();
  raw->scope->scopeKind = Scope::ScopeKind::FUNC;

  for (auto &a : decl->params) {
    a->accept(this);
  }

  if (!raw->isExtern) {
    decl->body->accept(this);
  }
}

void Builder::visit(VarDecl *decl) {
  auto symbol = make_unique<ValueSymbol>();
  symbol->name = decl->name;
  symbol->kind = ValueSymbol::Kind::VAR;
  symbol->node = decl;
  symbol->isRoot = decl->isRoot;
  symbol->modifier = decl->aModifier;
  symbol->nameSpan = decl->span;
  symbol->isConst = !decl->isMutable;
  auto raw = symbol.get();

  auto result = table.add(std::move(symbol));

  if (decl->isRoot) {
    if (!result.success) {
      switch (result.errorType) {
      case Result::DUPLICATED: {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S005);
        dia.labels = {
            {decl->span, "duplicate variable declared here", true},
            {table.getType(decl->name)->decl->span,
             "previous variable declared here", false},
        };
        dia.notes = {
            "variable names must be unique within the root",
        };
        engine.emit(dia);
        recover.recover();
      } break;

      case Result::RESERVED: {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S003);
        dia.labels = {
            {decl->span, "'" + decl->name + "'is a reserved identifier", true},
        };
        dia.notes = {
            {"reserved identifiers cannot be used in user declarations"}};
        engine.emit(dia);
        recover.recover();
        break;
      } break;

      case Result::UNKNOWN_SYMBOL:
        Error::internal(decl->span, "unknown symbol '" + decl->name + "'");
        break;

      case Result::NONE:
        break;
      }
    }
  } else {
    if (!result.success) {
      switch (result.errorType) {
      case Result::DUPLICATED: {
        if (decl->context == DeclContext::CLASSBODY) {
          auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S006);
          dia.labels = {
              {decl->span, "duplicate field declared here", true},
              {table.getType(decl->name)->decl->span,
               "previous field declared here", false},
          };
          dia.notes = {
              "field names must be unique within the same type",
          };
          engine.emit(dia);
        } else {
          auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S005);
          dia.labels = {
              {decl->span, "duplicate variable declared here", true},
              {table.getType(decl->name)->decl->span,
               "previous variable declared here", false},
          };
          dia.notes = {
              "variable names must be unique within the same scope",
          };
          engine.emit(dia);
        }

        recover.recover();
      } break;

      case Result::RESERVED: {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S003);
        dia.labels = {
            {decl->span, "'" + decl->name + "'is a reserved identifier", true},
        };
        dia.notes = {
            {"reserved identifiers cannot be used in user declarations"}};
        engine.emit(dia);
        recover.recover();
        break;
      } break;

      case Result::UNKNOWN_SYMBOL:
        Error::internal(decl->span, "unknown symbol '" + decl->name + "'");
        break;

      case Result::NONE:
        break;
      }
    }
  }

  decl->symbol = raw;

  if (decl->init) {
    decl->init->accept(this);
  }
}

void Builder::visit(TypeNode *) {}
void Builder::visit(ASTNode *) {}
void Builder::visit(Param *a) {
  auto s = make_unique<ParamSymbol>();
  s->name = a->name;
  s->kind = ValueSymbol::Kind::PARAM;
  s->node = a;
  s->nameSpan = a->span;
  auto r = s.get();
  auto re = table.add(std::move(s));

  if (!re.success) {
    switch (re.errorType) {
    case Result::DUPLICATED: {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S005);
      dia.labels = {
          {a->span, "duplicate variable declared here", true},
          {table.getType(a->name)->decl->span,
           "previous variable declared here", false},
      };
      dia.notes = {
          "variable names must be unique within the same scope",
      };
      engine.emit(dia);
    } break;

    case Result::RESERVED: {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S003);
      dia.labels = {
          {a->span, "'" + a->name + "'is a reserved identifier", true},
      };
      dia.notes = {
          {"reserved identifiers cannot be used in user declarations"}};
      engine.emit(dia);
      recover.recover();
      break;
    } break;

    case Result::UNKNOWN_SYMBOL:
      Error::internal(a->span, "unknown symbol '" + a->name + "'");
      break;

    case Result::NONE:
      break;
    }
  }
  a->symbol = r;
}

void Builder::visit(InitDecl *decl) {
  auto symbol = make_unique<MethodSymbol>();

  symbol->name = decl->name;
  symbol->decl = decl;
  symbol->owner = currentType;
  symbol->methodKind = MethodKind::Init;
  symbol->returnType = table.registry.getBuilt("void");
  symbol->module = table.moudle;
  symbol->path = table.registry.getCurrentFile()->path;
  auto raw = symbol.get();

  table.registry.addInit(std::move(symbol));

  decl->methodSymbol = raw;

  ScopeGuard _(table);

  raw->scope = table.scopeManger.current();
  table.scopeManger.current()->scopeKind = Scope::ScopeKind::INIT;
  raw->isOverride = decl->prefix.isOverride;

  for (auto &a : decl->params) {
    auto s = make_unique<ParamSymbol>();
    s->name = a->name;
    s->kind = ValueSymbol::Kind::PARAM;
    s->node = a.get();
    auto r = s.get();
    auto re = table.add(std::move(s));

    if (!re.success) {
      switch (re.errorType) {
      case Result::DUPLICATED: {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S005);
        dia.labels = {
            {a->span, "duplicate variable declared here", true},
            {table.getType(a->name)->decl->span,
             "previous variable declared here", false},
        };
        dia.notes = {
            "variable names must be unique within the same scope",
        };
        engine.emit(dia);
      } break;

      case Result::RESERVED: {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S003);
        dia.labels = {
            {decl->span, "'" + decl->name + "'is a reserved identifier", true},
        };
        dia.notes = {
            {"reserved identifiers cannot be used in user declarations"}};
        engine.emit(dia);
        recover.recover();
        break;
      } break;

      case Result::UNKNOWN_SYMBOL:
        Error::internal(a.get()->span,
                        "unknown symbol '" + a.get()->name + "'");
        break;

      case Result::NONE:
        break;
      }
    }
    a->symbol = r;
    a->type->accept(this);
  }

  decl->body->accept(this);
}

void Builder::visit(OnDestroyDecl *decl) {
  auto symbol = make_unique<MethodSymbol>();

  symbol->name = decl->name;
  symbol->decl = decl;
  symbol->owner = currentType;
  symbol->methodKind = MethodKind::OnDestroy;
  symbol->returnType = table.registry.getBuilt("void");
  symbol->module = table.moudle;
  symbol->path = table.registry.getCurrentFile()->path;
  auto raw = symbol.get();

  if (!table.registry.addOnDestroy(std::move(symbol))) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S007);
    dia.labels = {
        {decl->span, "duplicate onDestroy declared here", true},
        {table.getType(decl->name)->decl->span,
         "previous onDestroy declared here", false},
    };
    dia.notes = {
        "method signatures must be unique within the same type",
    };
    engine.emit(dia);
  }

  decl->methodSymbol = raw;

  ScopeGuard _(table);

  raw->scope = table.scopeManger.current();
  table.scopeManger.current()->scopeKind = Scope::ScopeKind::ONDESTROY;

  decl->body->accept(this);
}

void Builder::visit(ImportDecl *decl) {
  vector<string> vec;
  for (auto s : decl->path) {
    vec.push_back(s.str);
  }
  decl->sPath = {std::move(vec)};
}