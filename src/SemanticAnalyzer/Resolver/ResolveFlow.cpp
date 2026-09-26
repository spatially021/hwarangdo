#include "hrd/AST/CaseAble.h"
#include "hrd/AST/Stmt.h"
#include "hrd/SemanticAnalyzer/Resolver.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/diagnostic/Diagnostic.h"
#include "hrd/util/Guard.h"

void Resolver::visit(Range *expr) {
  expr->from->accept(this);
  if (!isa<IntType>(expr->from->resolvedType)) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S054);
    dia.labels = {
        {expr->from->span,
         "range start has type '" + expr->from->resolvedType->name + "'", true},
    };
    dia.notes = {
        "only integer ranges are currently supported",
    };
    dia.helps = {
        "use an integer value as the range start",
    };
    engine.emit(dia);
    recover.recover();
  }

  expr->to->accept(this);
  if (!isa<IntType>(expr->to->resolvedType)) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S054);
    dia.labels = {
        {expr->to->span,
         "range end has type '" + expr->to->resolvedType->name + "'", true},
    };
    dia.notes = {
        "only integer ranges are currently supported",
    };
    dia.helps = {
        "use an integer value as the range end",
    };
    engine.emit(dia);
    recover.recover();
  }

  if (expr->step == nullptr) {
    Token step;
    step.span = expr->span;
    step.kind = TKind::LIT_INT;
    step.text = "1";
    expr->step = make_shared<LiteralExpr>(expr->span, step, "1");
  }

  expr->step->accept(this);
  if (!isa<IntType>(expr->step->resolvedType)) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S055);
    dia.labels = {
        {expr->step->span,
         "range step has type '" + expr->step->resolvedType->name + "'", true},
    };
    dia.notes = {
        "a range step must have an integer type",
    };
    dia.helps = {
        "use an integer value for the range step",
    };
    engine.emit(dia);
    recover.recover();
  }

  expr->resolvedType = expr->from->resolvedType;
}

void Resolver::visit(CaseValueExpr *expr) {
  if (auto lit = dynamic_cast<LiteralExpr *>(expr->value.get())) {
    expr->value->accept(this);
    expr->resolvedType = expr->value->resolvedType;

    CaseKey key = CaseKey(lit->resolvedLit);

    if (!currentSwitch->caseKeys.insert(key).second) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S056);
      dia.labels = {
          {expr->value->span, "this case value is already used", true},
      };
      if (currentSwitch->sKind == SwitchKind::Switch) {
        dia.notes = {
            "each case value must be unique within the same switch",
        };
      } else {
        dia.notes = {
            "each case value must be unique within the same match",
        };
      }

      dia.helps = {
          "remove this case or use a different value",
      };
      engine.emit(dia);
      recover.recover();
    }

    return;
  }

  if (dynamic_cast<DefaultValueExpr *>(expr->value.get())) {
    expr->isWildCard = true;
    return;
  }

  TypeSymbol *target = getTargetType();
  if (target == nullptr) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S057);
    dia.labels = {
        {expr->value->span,
         "an enum variant cannot be resolved for this target", true},
    };
    dia.notes = {
        "enum variant cases require the switch or match target to have an enum "
        "type",
    };
    dia.helps = {
        "use a literal case value or change the target expression to an enum "
        "value",
    };
    engine.emit(dia);
    recover.recover();
  }

  string name;

  if (auto n = dynamic_cast<NameExpr *>(expr->value.get())) {
    name = n->name;
  } else if (auto m = dynamic_cast<MemberExpr *>(expr->value.get())) {
    name = m->member;
    m->object->accept(this);

    if (m->object->resolvedType != target) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S058);
      dia.labels = {
          {m->object->span,
           "this expression has enum type '" + m->object->resolvedType->name +
               "'",
           true},
      };
      dia.notes = {
          "the case variant must belong to enum type '" + target->name + "'",
      };
      dia.helps = {
          "use a variant declared by '" + target->name + "'",
      };
      engine.emit(dia);
      recover.recover();
    }
  } else if (auto c = dynamic_cast<CallExpr *>(expr->value.get())) {
    name = c->methodName;

    if (c->receiver != nullptr) {
      c->receiver->accept(this);

      if (c->receiver->resolvedType != target) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S058);
        dia.labels = {
            {c->receiver->span,
             "this expression has enum type '" +
                 c->receiver->resolvedType->name + "'",
             true},
        };
        dia.notes = {
            "the case variant must belong to enum type '" + target->name + "'",
        };
        dia.helps = {
            "use a variant declared by '" + target->name + "'",
        };
        engine.emit(dia);
        recover.recover();
      }
    }
  } else {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S059);
    dia.labels = {
        {expr->value->span, "this expression cannot be used as a case value",
         true},
    };
    dia.notes = {
        "case values must be literals or enum variants",
    };
    dia.helps = {
        "replace this expression with a literal or enum variant",
    };
    engine.emit(dia);
    recover.recover();
  }

  GenericSubstitution substitution;
  TypeSymbol *payloadType = nullptr;
  EnumVariantSymbol *variant = nullptr;
  if (auto e = dyn_cast<EnumType>(target)) {
    auto it = e->variantMap.find(name);
    if (it == e->variantMap.end()) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S028);
      dia.labels = {
          {expr->value->span,
           "enum '" + e->name + "' has no variant named '" + name + "'", true},
      };
      dia.notes = {
          "the case value must reference a variant declared by the target enum",
      };
      dia.helps = {
          "use a variant declared by '" + e->name + "'",
      };
      engine.emit(dia);
      recover.recover();
    }
    payloadType = it->second->payloadType;
    variant = it->second;
  } else if (auto generic = dyn_cast<GenericSymbol>(target)) {
    substitution = makeGenericSubstitution(generic);
    auto origin = dyn_cast<EnumType>(generic->origin);
    auto it = origin->variantMap.find(name);
    if (it == origin->variantMap.end()) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S028);
      dia.labels = {
          {expr->value->span,
           "enum '" + origin->name + "' has no variant named '" + name + "'",
           true},
      };
      dia.notes = {
          "the case value must reference a variant declared by the target enum",
      };
      dia.helps = {
          "use a variant declared by '" + origin->name + "'",
      };
      engine.emit(dia);
      recover.recover();
    }
    payloadType = substituteGenericType(it->second->payloadType, substitution);
    variant = it->second;
  }

  if (expr->arg) {
    if (variant->payloadType == nullptr) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S023);
      dia.labels = {
          {expr->arg->span, "variant '" + name + "' does not declare a payload",
           true},
      };
      dia.helps = {
          "remove the payload binding from this case",
      };
      engine.emit(dia);
      recover.recover();
    }

    if (auto n = dynamic_cast<NameExpr *>(expr->arg.get())) {
      unique_ptr<ValueSymbol> symbol = make_unique<ValueSymbol>();
      symbol->typeSymbol = payloadType;
      symbol->isPayload = true;
      symbol->isRoot = false;
      symbol->kind = ValueSymbol::Kind::VAR;
      symbol->owner = table.scopeManger.current();
      symbol->name = n->name;
      expr->payloadType = payloadType;

      auto raw = symbol.get();
      expr->payload = raw;

      auto iter = table.scopeManger.current()->value.find(n->name);
      if (iter == table.scopeManger.current()->value.end()) {
        table.scopeManger.current()->value.emplace(n->name, std::move(symbol));
      } else {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S060);
        dia.labels = {
            {expr->arg->span,
             "payload binding '" + n->name + "' is already declared", true},
        };
        dia.notes = {
            "a payload binding cannot reuse a variable name in the same scope",
        };
        dia.helps = {
            "use a different name for this payload binding",
        };
        engine.emit(dia);
        recover.recover();
      }
    } else {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S061);
      dia.labels = {
          {expr->arg->span, "expected a name for the variant payload", true},
      };
      dia.notes = {
          "a case payload introduces a variable bound to the variant payload",
      };
      dia.helps = {
          "replace this expression with a variable name",
      };
      engine.emit(dia);
      recover.recover();
    }
  }

  CaseKey key = CaseKey(variant);

  if (!currentSwitch->caseKeys.insert(key).second) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S056);
    dia.labels = {
        {expr->value->span, "this enum variant is already used", true},
    };
    if (currentSwitch->sKind == SwitchKind::Switch) {
      dia.notes = {
          "each enum variant may appear only once within the same switch",
      };
    } else {
      dia.notes = {
          "each enum variant may appear only once within the same match",
      };
    }

    dia.helps = {
        "remove this case or use a different enum variant",
    };
    engine.emit(dia);
    recover.recover();
  }

  currentSwitch->usedVariants.insert(variant);
  expr->variant = variant;
}

TypeSymbol *Resolver::getTargetType() {

  auto type = currentSwitch->value->resolvedType;
  if (auto e = dyn_cast<EnumType>(type)) {
    return e;
  }

  if (auto g = dyn_cast<GenericSymbol>(type)) {
    if (dyn_cast<EnumType>(g->origin)) {
      return g;
    }
  }

  return nullptr;
}

void Resolver::visit(MatchExpr *expr) {
  ScopeGuard _(table, expr->blockScope);

  auto prev = currentSwitch;
  currentSwitch = expr;

  expr->value->accept(this);

  TypeSymbol *matchType = nullptr;

  for (unsigned i = 0; i < expr->clauses.size(); ++i) {
    auto &c = expr->clauses[i];
    c->accept(this);

    if (matchType == nullptr) {
      matchType = c->transferType;
    } else if (!isAssignable(matchType, c->transferType)) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S062);
      dia.labels = {
          {c->span, "this case transfers type '" + c->transferType->name + "'",
           true},
      };
      dia.notes = {
          "previous cases transfer type '" + matchType->name + "'",
          "all match cases must transfer compatible values",
      };
      dia.helps = {
          "change this case to transfer a value compatible with '" +
              matchType->name + "'",
      };
      engine.emit(dia);
      recover.recover();
    }

    if (c->isWildCard) {
      if (i != expr->clauses.size() - 1) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S063);
        dia.labels = {
            {c->span, "wildcard case appears before another case", true},
        };
        dia.notes = {
            "the wildcard case matches every value not handled by an earlier "
            "case",
        };
        dia.helps = {
            "move this wildcard case to the end of the match",
        };
        engine.emit(dia);
        recover.recover();
      }

      expr->hasDefault = true;
    }
  }

  checkSwitchValue(expr, expr->span);

  expr->resolvedType = matchType;
  currentSwitch = prev;
}