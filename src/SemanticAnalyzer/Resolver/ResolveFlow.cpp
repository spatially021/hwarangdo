#include "hrd/AST/Stmt.h"
#include "hrd/SemanticAnalyzer/Resolver.h"
#include "hrd/util/Guard.h"
#include "hrd/util/diagnostic/Diagnostic.h"

void Resolver::visit(Range *expr) {
  expr->from->accept(this);
  if (!table.isInt(expr->from->resolvedType)) {
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
  if (!table.isInt(expr->to->resolvedType)) {
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
  if (!table.isInt(expr->step->resolvedType)) {
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

    if (auto s = dynamic_cast<SwitchStmt *>(currentSwitch)) {
      if (!s->caseKeys.insert(key).second) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S056);
        dia.labels = {
            {expr->value->span, "this case value is already used", true},
        };
        dia.notes = {
            "each case value must be unique within the same switch",
        };
        dia.helps = {
            "remove this case or use a different value",
        };
        engine.emit(dia);
        recover.recover();
      }
    }

    if (auto m = dynamic_cast<MatchExpr *>(currentSwitch)) {
      if (!m->caseKeys.insert(key).second) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S056);
        dia.labels = {
            {expr->value->span, "this case value is already used", true},
        };
        dia.notes = {
            "each case value must be unique within the same match",
        };
        dia.helps = {
            "remove this case or use a different value",
        };
        engine.emit(dia);
        recover.recover();
      }
    }

    return;
  }

  if (dynamic_cast<DefaultValueExpr *>(expr->value.get())) {
    expr->isWildCard = true;
    return;
  }

  TypeSymbol *enumTarget = getTargetType();
  if (enumTarget == nullptr) {
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

    if (m->object->resolvedType != enumTarget) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S058);
      dia.labels = {
          {m->object->span,
           "this expression has enum type '" + m->object->resolvedType->name +
               "'",
           true},
      };
      dia.notes = {
          "the case variant must belong to enum type '" + enumTarget->name +
              "'",
      };
      dia.helps = {
          "use a variant declared by '" + enumTarget->name + "'",
      };
      engine.emit(dia);
      recover.recover();
    }
  } else if (auto c = dynamic_cast<CallExpr *>(expr->value.get())) {
    name = c->methodName;

    if (c->receiver != nullptr) {
      c->receiver->accept(this);

      if (c->receiver->resolvedType != enumTarget) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S058);
        dia.labels = {
            {c->receiver->span,
             "this expression has enum type '" +
                 c->receiver->resolvedType->name + "'",
             true},
        };
        dia.notes = {
            "the case variant must belong to enum type '" + enumTarget->name +
                "'",
        };
        dia.helps = {
            "use a variant declared by '" + enumTarget->name + "'",
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

  auto it = enumTarget->variantMap.find(name);
  if (it == enumTarget->variantMap.end()) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S028);
    dia.labels = {
        {expr->value->span,
         "enum '" + enumTarget->name + "' has no variant named '" + name + "'",
         true},
    };
    dia.notes = {
        "the case value must reference a variant declared by the target enum",
    };
    dia.helps = {
        "use a variant declared by '" + enumTarget->name + "'",
    };
    engine.emit(dia);
    recover.recover();
  }

  if (expr->arg) {
    if (it->second->payloadType == nullptr) {
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
      symbol->typeSymbol = it->second->payloadType;
      symbol->isPayload = true;
      symbol->isRoot = false;
      symbol->kind = ValueSymbol::Kind::VAR;
      symbol->owner = table.getCurrent();
      symbol->name = n->name;

      expr->payloadType = it->second->payloadType;

      auto raw = symbol.get();
      expr->payload = raw;

      auto iter = table.getCurrent()->value.find(n->name);
      if (iter == table.getCurrent()->value.end()) {
        table.getCurrent()->value.emplace(n->name, std::move(symbol));
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

  CaseKey key = CaseKey(it->second);

  if (auto s = dynamic_cast<SwitchStmt *>(currentSwitch)) {
    if (!s->caseKeys.insert(key).second) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S056);
      dia.labels = {
          {expr->value->span, "this enum variant is already used", true},
      };
      dia.notes = {
          "each enum variant may appear only once within the same switch",
      };
      dia.helps = {
          "remove this case or use a different enum variant",
      };
      engine.emit(dia);
      recover.recover();
    }

    s->usedVariants.insert(it->second);
  }

  if (auto m = dynamic_cast<MatchExpr *>(currentSwitch)) {
    if (!m->caseKeys.insert(key).second) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S056);
      dia.labels = {
          {expr->value->span, "this enum variant is already used", true},
      };
      dia.notes = {
          "each enum variant may appear only once within the same match",
      };
      dia.helps = {
          "remove this case or use a different enum variant",
      };
      engine.emit(dia);
      recover.recover();
    }

    m->usedVariants.insert(it->second);
  }

  expr->variant = it->second;
}

TypeSymbol *Resolver::getTargetType() {
  if (auto s = dynamic_cast<SwitchStmt *>(currentSwitch)) {
    if (s->value->resolvedType->kind == TypeSymbol::TypeKind::ENUM) {
      return s->value->resolvedType;
    }
  }

  if (auto m = dynamic_cast<MatchExpr *>(currentSwitch)) {
    if (m->value->resolvedType->kind == TypeSymbol::TypeKind::ENUM) {
      return m->value->resolvedType;
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

  for (unsigned i = 0; i < expr->cases.size(); ++i) {
    auto &c = expr->cases[i];
    c->accept(this);

    if (!matchType) {
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
      if (i != expr->cases.size() - 1) {
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

  auto *targetType = expr->value->resolvedType;

  if (targetType->kind == TypeSymbol::TypeKind::ENUM) {
    if (expr->usedVariants.size() != targetType->variants.size() &&
        !expr->hasDefault) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S064);
      dia.labels = {
          {expr->span, "this match does not handle every enum variant", true},
      };
      dia.notes = {
          "match expressions must handle every possible value",
      };
      dia.helps = {
          "add the missing enum cases or add a final wildcard case",
      };
      engine.emit(dia);
      recover.recover();
    }
  } else if (targetType->kind == TypeSymbol::TypeKind::PRIMITIVE) {
    if (!expr->hasDefault) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S064);
      dia.labels = {
          {expr->span, "this match has no wildcard case", true},
      };
      dia.notes = {
          "primitive values cannot be exhaustively enumerated by case values",
      };
      dia.helps = {
          "add a final wildcard case",
      };
      engine.emit(dia);
      recover.recover();
    }
  } else {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S065);
    dia.labels = {
        {expr->value->span,
         "this expression has type '" + targetType->name + "'", true},
    };
    dia.notes = {
        "match expressions only support primitive and enum target values",
    };
    dia.helps = {
        "use a primitive or enum expression as the match target",
    };
    engine.emit(dia);
    recover.recover();
  }

  expr->resolvedType = matchType;
  currentSwitch = prev;
}