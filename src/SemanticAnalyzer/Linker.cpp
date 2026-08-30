#include "hrd/SemanticAnalyzer/Linker.h"
#include "hrd/AST/Decl.h"
#include "hrd/AST/Expr.h"
#include "hrd/AST/Stmt.h"
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
#include <memory>
#include <utility>
#include <vector>

Linker::Linker(LinkerContext &ctx)
    : table(ctx.table), engine(ctx.engine), recover(*this) {}

void Linker::visit(LiteralExpr *) {}
void Linker::visit(BinaryExpr *expr) {
  expr->left->accept(this);
  expr->right->accept(this);
}
void Linker::visit(NameExpr *) {}
void Linker::visit(UnaryExpr *expr) { expr->right->accept(this); }
void Linker::visit(CallExpr *expr) {
  if (expr->receiver != nullptr) {
    expr->receiver->accept(this);
  }
  for (auto &a : expr->arguments) {
    a->accept(this);
  }
}
void Linker::visit(AssignExpr *expr) {
  expr->target->accept(this);
  expr->value->accept(this);
}
void Linker::visit(MemberExpr *expr) { expr->object->accept(this); }
void Linker::visit(ArrayAccessExpr *expr) {
  expr->object->accept(this);
  expr->index->accept(this);
}
void Linker::visit(TernaryExpr *expr) {
  expr->conditon->accept(this);
  expr->then->accept(this);
  expr->else_->accept(this);
}
void Linker::visit(ThisExpr *) {}
void Linker::visit(SuperExpr *) {}
void Linker::visit(RootExpr *) {}
void Linker::visit(SelfExpr *) {}
void Linker::visit(CastExpr *) {}
void Linker::visit(BuiltInNameExpr *) {}
void Linker::visit(SpawnExpr *expr) {
  expr->left->accept(this);
  expr->spawnType->accept(this);
}
void Linker::visit(ViewExpr *expr) {
  expr->left->accept(this);
  expr->target->accept(this);
}
void Linker::visit(DestroyExpr *expr) {
  expr->storage->accept(this);
  expr->target->accept(this);
}
void Linker::visit(QuitExpr *) {}
void Linker::visit(DefaultValueExpr *) {}
void Linker::visit(Range *expr) {
  expr->from->accept(this);
  expr->to->accept(this);
  if (expr->step) {
    expr->step->accept(this);
  }
}
void Linker::visit(CaseValueExpr *expr) {
  expr->value->accept(this);
  if (expr->arg) {
    expr->arg->accept(this);
  }
}
void Linker::visit(MatchExpr *expr) {
  ScopeGuard _(table, expr->blockScope);
  expr->value->accept(this);
  for (auto &c : expr->cases) {
    c->accept(this);
  }
}
void Linker::visit(ArrayLiteralExpr *expr) {
  for (auto &e : expr->elements) {
    e->accept(this);
  }
}

// Statement Linker::visitor methods
void Linker::visit(ExprStmt *stmt) { stmt->expr->accept(this); }
void Linker::visit(BlockStmt *stmt) {
  ScopeGuard _(table, stmt->blockScope);
  for (auto s : stmt->statements) {
    s->accept(this);
  }
}
void Linker::visit(IfStmt *stmt) {
  stmt->thenBranch->accept(this);
  if (stmt->elseBranch != nullptr) {
    stmt->elseBranch->accept(this);
  }
}
void Linker::visit(ForStmt *stmt) {
  ScopeGuard _(table, stmt->blockScope);
  stmt->initializer->accept(this);
  stmt->range->accept(this);
  stmt->body->accept(this);
}
void Linker::visit(WhileStmt *stmt) { stmt->body->accept(this); }
void Linker::visit(SwitchStmt *stmt) {
  ScopeGuard _(table, stmt->blockScope);
  for (auto &c : stmt->clauses) {
    c->accept(this);
  }
}
void Linker::visit(Case *c) { c->body->accept(this); }
void Linker::visit(ReturnStmt *stmt) {
  if (stmt->value != nullptr) {
    stmt->value->accept(this);
  }
}
void Linker::visit(ValueTransferStmt *stmt) { stmt->value->accept(this); }
void Linker::visit(BreakStmt *) {}
void Linker::visit(ContinueStmt *) {}
void Linker::visit(DeclStmt *stmt) { stmt->decl->accept(this); }
void Linker::visit(EmptyStmt *) {}

// declare Linker::visitor methods
void Linker::visit(ClassDecl *decl) {

  if (decl->symbol->type == Symbol::SymbolType::MAIN) {
    auto symbol = static_cast<MainSymbol *>(decl->symbol);
    auto &bucket = symbol->memberScope->methodMap["update"];
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

    bucket = symbol->memberScope->inits;
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
    auto s = decl->baseClass.value();
    if (table.isType(s.str)) {
      auto symbol = table.getType(s.str);
      symbol->decl->isExtended = true;
      if (symbol->kind != TypeSymbol::TypeKind::CLASS) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S012);
        dia.labels = {
            {s.span, "this type is not a class", true},

        };
        engine.emit(dia);
        recover.recover();
      }
      decl->symbol->base = symbol;
    } else {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S013);
      dia.labels = {
          {s.span, "type '" + s.str + "' not found ", true},

      };
      engine.emit(dia);
      recover.recover();
    }
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
    if (symbol->kind != TypeSymbol::TypeKind::TRAIT) {
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
  for (auto &v : decl->variants) {
    if (v->payload.has_value()) {
      auto t = v->payload.value().get();
      auto s = table.getType(t);
      if (s == nullptr) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S013);
        dia.labels = {
            {v->token.span, "type '" + t->type + "' not found ", true},
        };
        engine.emit(dia);
        recover.recover();
      }

      if (s->kind == TypeSymbol::TypeKind::CLASS) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S016);
        dia.labels = {
            {decl->span, "entity type used here", true},
        };
        dia.notes = {{"enum variant payloads cannot contain entity types"}};
        engine.emit(dia);
        recover.recover();
      }
      t->resolved = s;
      v->symbol->payloadType = table.getType(t);
    }
  }
}
void Linker::visit(ImplDecl *decl) {
  auto impl = table.registry.getImpl(decl);
  ScopeGuard _(table, impl->memberScope);
  auto s = decl->target;

  if (!table.isType(s.str)) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S013);
    dia.labels = {
        {s.span, "type '" + s.str + "' not found ", true},

    };
    engine.emit(dia);
    recover.recover();
  }
  auto symbol = table.getType(s.str);
  if (symbol->kind != TypeSymbol::TypeKind::STRUCT) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S017);
    dia.labels = {
        {decl->span, "'" + s.str + "' is not a struct type", true},
    };
    engine.emit(dia);
    recover.recover();
  }
  decl->importTarget = symbol;

  for (auto &c : decl->traits) {
    auto t = table.getType(c.str);
    if (t->kind != TypeSymbol::TypeKind::TRAIT) {
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

    for (auto &it : t->memberScope->methodMap) {
      for (auto sig : it.second) {
        decl->sigs.push_back(sig);
      }
    }
  }

  TypeContextGuard __(currentType, symbol);

  impl->target = symbol;
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
    if (auto [result, span] = symbol->addMethod(methodSymbol); !result) {
      auto it = symbol->memberScope->methodMap.find(a->name);
      if (it == symbol->memberScope->methodMap.end()) {
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
    methodSymbol->selfScope = symbol->memberScope;
  }
}

void Linker::visit(TraitDecl *decl) {
  ScopeGuard _(table, decl->symbol->memberScope);
  for (auto &s : decl->traitSigs) {
    s->accept(this);
  }
}

void Linker::visit(FuncDecl *decl) {

  if (decl->returnType == nullptr) {
    Error::internal(decl->span, "return ast node is nullptr");
  }

  decl->returnType->accept(this);
  decl->methodSymbol->returnType = decl->returnType->resolved;

  ScopeGuard _(table, decl->methodSymbol->scope);
  for (auto &p : decl->params) {
    p->accept(this);
    p->symbol->typeSymbol = p->type->resolved;
    decl->methodSymbol->params.push_back(p->symbol);
  }

  auto it = currentType->memberScope->methodMap.find(decl->methodSymbol->name);
  if (it == currentType->memberScope->methodMap.end()) {
    Error::internal(decl->span, "fail to find method map");
  }

  auto &bucket = it->second;
  auto raw = decl->methodSymbol;
  if (auto [result, span] = Helper::hasSameMethodSig(bucket, raw); result) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S018);
    dia.labels = {
        {raw->decl->span, "duplicate impl method declared here", true},
        {span, "previous impl method declared here", false}};
    engine.emit(dia);
    recover.recover();
  }

  unique_ptr<ValueSymbol> selfReceiver = make_unique<ValueSymbol>();
  selfReceiver->typeSymbol = currentType;
  selfReceiver->name = decl->name + "self";
  auto rawSelf = selfReceiver.get();

  table.registry.addSelf(std::move(selfReceiver));
  raw->selfReceiver = rawSelf;

  decl->body->accept(this);
}
void Linker::visit(VarDecl *decl) {
  decl->type->accept(this);
  decl->symbol->typeSymbol = decl->type->resolved;
  if (decl->init) {
    decl->init->accept(this);
  }
}

void Linker::visit(TypeNode *type) {
  TypeResolverContext context = {engine, table, recover};
  TypeResolver::resolveTypeNode(type, context);
}
void Linker::visit(ASTNode *) {}

void Linker::visit(TraitSig *sig) {
  for (auto &p : sig->params) {
    p->accept(this);
    sig->symbol->params.push_back(p->symbol);
  }
  sig->type->accept(this);
  sig->symbol->returnType = sig->type->resolved;
}
void Linker::visit(Param *param) {
  param->type->accept(this);
  param->symbol->typeSymbol = param->type->resolved;
  if (param->defaultValue.has_value()) {
    auto expr = param->defaultValue.value().get();

    if (auto lit = dynamic_cast<LiteralExpr *>(expr)) {
      param->symbol->defaultValue = lit;
    } else if (auto call = dynamic_cast<CallExpr *>(expr)) {
      param->symbol->defaultValue = call;
    } else {
      Error::internal(param->span, "illegal defaultValue ast kind");
    }
  }
}

void Linker::visit(InitDecl *decl) {
  ScopeGuard _(table, decl->methodSymbol->scope);

  for (auto &p : decl->params) {
    p->accept(this);
    p->symbol->typeSymbol = p->type->resolved;
    decl->methodSymbol->params.push_back(p->symbol);
  }
  decl->body->accept(this);
}

void Linker::visit(OnDestroyDecl *decl) {
  ScopeGuard _(table, decl->methodSymbol->scope);
  decl->body->accept(this);
}

void Linker::visit(ImportDecl *) {}