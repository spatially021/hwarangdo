#include "hrd/AST/Expr.h"
#include "hrd/AST/Stmt.h"
#include "hrd/IR/HIR/HIRBuilder.h"
#include "hrd/IR/HIR/HIRDecl.h"
#include "hrd/IR/HIR/HIRExpr.h"
#include "hrd/IR/HIR/HIRHelper.h"
#include "hrd/IR/HIR/HIRPattern.h"
#include "hrd/IR/HIR/HIRStmt.h"
#include "hrd/IR/HIR/HIRSymbol.h"
#include "hrd/util/Error.h"
#include "hrd/util/Guard.h"
#include <memory>
#include <utility>

unique_ptr<HIRBlockStmt> HIRBuilder::lowerBlock(BlockStmt *stmt) {
  if (stmt == nullptr) {
    Error::internal("block statement is nullptr");
  }

  auto block = make_unique<HIRBlockStmt>(stmt->span);
  BlockGuard _(currentBlock, block.get());

  for (auto &statement : stmt->statements) {
    if (statement == nullptr) {
      Error::internal(stmt->span, "block contains nullptr statement");
    }

    statement->accept(this);
  }

  return block;
}

unique_ptr<HIRBlockStmt> HIRBuilder::lowerStmtAsBlock(Stmt *stmt) {
  if (stmt == nullptr) {
    Error::internal("statement is nullptr");
  }

  if (auto *block = dynamic_cast<BlockStmt *>(stmt)) {
    return lowerBlock(block);
  }

  auto block = make_unique<HIRBlockStmt>(stmt->span);
  BlockGuard _(currentBlock, block.get());
  stmt->accept(this);

  return block;
}

unique_ptr<HIRStmt> HIRBuilder::lowerFor(ForStmt *stmt) {
  if (stmt == nullptr) {
    Error::internal("for statement is nullptr");
  }

  if (stmt->initializer == nullptr) {
    Error::internal(stmt->span, "for initializer is nullptr");
  }

  if (stmt->range == nullptr) {
    Error::internal(stmt->span, "for range is nullptr");
  }

  if (stmt->body == nullptr) {
    Error::internal(stmt->span, "for body is nullptr");
  }

  HIRLocal *local = nullptr;

  if (auto *declStmt = dynamic_cast<DeclStmt *>(stmt->initializer.get())) {
    if (auto *varDecl = dynamic_cast<VarDecl *>(declStmt->decl.get())) {
      local = lowerLocal(varDecl);
    } else {
      Error::internal(stmt->initializer->span,
                      "for initializer declaration is not VarDecl");
    }
  } else {
    Error::internal(stmt->initializer->span, "for initializer is not DeclStmt");
  }

  if (local == nullptr) {
    Error::internal(stmt->initializer->span,
                    "for index local lowering returned nullptr");
  }

  if (stmt->range->from == nullptr) {
    Error::internal(stmt->range->span, "for range start is nullptr");
  }

  if (stmt->range->to == nullptr) {
    Error::internal(stmt->range->span, "for range end is nullptr");
  }

  if (stmt->range->step == nullptr) {
    Error::internal(stmt->range->span, "for range step is nullptr");
  }

  auto from = lowerExpr(stmt->range->from.get());
  auto to = lowerExpr(stmt->range->to.get());
  auto step = lowerExpr(stmt->range->step.get());
  auto body = lowerStmtAsBlock(stmt->body.get());

  if (from == nullptr) {
    Error::internal(stmt->range->from->span,
                    "for range start lowering returned nullptr");
  }

  if (to == nullptr) {
    Error::internal(stmt->range->to->span,
                    "for range end lowering returned nullptr");
  }

  if (step == nullptr) {
    Error::internal(stmt->range->step->span,
                    "for range step lowering returned nullptr");
  }

  if (body == nullptr) {
    Error::internal(stmt->body->span, "for body lowering returned nullptr");
  }

  return make_unique<HIRForRangeStmt>(stmt->span, local, std::move(from),
                                      std::move(to), std::move(step),
                                      std::move(body));
}

unique_ptr<HIRStmt> HIRBuilder::lowerIf(IfStmt *stmt) {
  if (stmt == nullptr) {
    Error::internal("if statement is nullptr");
  }

  if (stmt->condition == nullptr) {
    Error::internal(stmt->span, "if condition is nullptr");
  }

  if (stmt->thenBranch == nullptr) {
    Error::internal(stmt->span, "if then branch is nullptr");
  }

  auto loweredCondition = lowerExpr(stmt->condition.get());

  if (loweredCondition == nullptr) {
    Error::internal(stmt->condition->span,
                    "if condition lowering returned nullptr");
  }

  unique_ptr<HIRValueExpr> condition;

  if (auto *value = dynamic_cast<HIRValueExpr *>(loweredCondition.get())) {
    condition.reset(value);
  } else if (auto *place =
                 dynamic_cast<HIRPlaceExpr *>(loweredCondition.get())) {
    condition = load(unique_ptr<HIRPlaceExpr>(place));
  } else {
    Error::internal(stmt->condition->span,
                    "if condition is neither place nor value expression");
  }

  if (condition == nullptr) {
    Error::internal(stmt->condition->span, "if condition value is nullptr");
  }

  auto thenBlock = lowerStmtAsBlock(stmt->thenBranch.get());
  auto elseBlock =
      stmt->elseBranch ? lowerStmtAsBlock(stmt->elseBranch.get()) : nullptr;

  return make_unique<HIRIfStmt>(stmt->span, std::move(condition),
                                std::move(thenBlock), std::move(elseBlock));
}

unique_ptr<HIRCase> HIRBuilder::lowerCase(Case *stmt) {
  if (stmt == nullptr) {
    Error::internal("case statement is nullptr");
  }

  vector<unique_ptr<HIRCasePattern>> selectors;
  selectors.reserve(stmt->values.size());

  for (auto &value : stmt->values) {
    if (value == nullptr) {
      Error::internal(stmt->span, "case value is nullptr");
    }

    auto *caseValue = dynamic_cast<CaseValueExpr *>(value.get());
    if (caseValue == nullptr) {
      Error::internal(value->span, "case value is not CaseValueExpr");
    }

    auto selector = lowerCaseValue(caseValue);
    if (selector == nullptr) {
      Error::internal(value->span, "case pattern lowering returned nullptr");
    }

    selectors.push_back(std::move(selector));
  }

  HIRDefaultKind defaultKind = HIRDefaultKind::None;

  if (stmt->isDefault) {
    defaultKind = HIRDefaultKind::Default;
  } else if (stmt->isWildCard) {
    defaultKind = HIRDefaultKind::WildCard;
  }

  if (stmt->body == nullptr) {
    Error::internal(stmt->span, "case body is nullptr");
  }

  auto body = lowerStmtAsBlock(stmt->body.get());

  return make_unique<HIRCase>(stmt->span, std::move(selectors), std::move(body),
                              defaultKind);
}

unique_ptr<HIRCasePattern> HIRBuilder::lowerCaseValue(CaseValueExpr *expr) {
  if (expr == nullptr) {
    Error::internal("case value expression is nullptr");
  }

  if (expr->isWildCard) {
    return make_unique<HIRCasePattern>(expr->span, HIRWildcardCase());
  }

  if (expr->value == nullptr) {
    Error::internal(expr->span, "case value expression value is nullptr");
  }

  if (auto *literal = dynamic_cast<LiteralExpr *>(expr->value.get())) {
    auto loweredLiteral = lowerLiteral(literal);

    if (loweredLiteral == nullptr) {
      Error::internal(literal->span, "literal case lowering returned nullptr");
    }

    auto *literalExpr = dynamic_cast<HIRLiteralExpr *>(loweredLiteral.get());

    if (literalExpr == nullptr) {
      Error::internal(literal->span,
                      "literal case did not lower to HIRLiteralExpr");
    }

    HIRLiteralCase literalPattern{
        unique_ptr<HIRLiteralExpr>(literalExpr),
    };

    return make_unique<HIRCasePattern>(expr->span, std::move(literalPattern));
  }

  if (expr->variant == nullptr) {
    Error::internal(expr->value->span,
                    "enum case value has no resolved variant");
  }

  if (dynamic_cast<NameExpr *>(expr->value.get())) {
    auto typeIt = program->typeDeclMap.find(expr->variant->typeSymbol);

    if (typeIt == program->typeDeclMap.end() || typeIt->second == nullptr) {
      Error::internal(expr->value->span,
                      "failed to find enum HIR type declaration");
    }

    auto variantIt = program->variantMap.find(expr->variant);

    if (variantIt == program->variantMap.end() ||
        variantIt->second == nullptr) {
      Error::internal(expr->value->span, "failed to find HIR enum variant");
    }

    HIRUnitCase unit{variantIt->second};

    return make_unique<HIRCasePattern>(expr->span, unit);
  }

  if (auto *member = dynamic_cast<MemberExpr *>(expr->value.get())) {
    if (!isTypeReceiver(member->object.get())) {
      Error::internal(expr->span, "enum case member has non-type receiver");
    }

    auto *type = member->object->resolvedType;

    if (type == nullptr) {
      Error::internal(member->object->span,
                      "enum case receiver type is nullptr");
    }

    auto typeIt = program->typeDeclMap.find(type);

    if (typeIt == program->typeDeclMap.end() || typeIt->second == nullptr) {
      Error::internal(expr->span, "failed to find enum HIR type declaration");
    }

    if (typeIt->second->typeDeclKind != HIRTypeDeclKind::Enum) {
      Error::internal(expr->span, "enum case receiver is not enum type");
    }

    auto variantIt = program->variantMap.find(expr->variant);

    if (variantIt == program->variantMap.end() ||
        variantIt->second == nullptr) {
      Error::internal(expr->span, "failed to find HIR enum variant");
    }

    HIRUnitCase unit{variantIt->second};

    return make_unique<HIRCasePattern>(expr->span, unit);
  }

  HIRLocal *binding = nullptr;

  if (expr->payload != nullptr) {
    auto local = make_unique<HIRLocal>();

    local->id = allocLocalID();
    local->isInitialized = true;
    local->isMutable = false;
    local->name = expr->payload->name;
    local->symbol = expr->payload;
    local->type =
        HIRHelper::lowerType(program, source, expr->payload->typeSymbol);
    local->isCaseValue = true;

    if (local->type == nullptr) {
      Error::internal("failed to lower payload binding type");
    }

    binding = local.get();
    bindLocal(expr->payload, std::move(local));
  }

  if (dynamic_cast<CallExpr *>(expr->value.get())) {
    auto typeIt = program->typeDeclMap.find(expr->variant->typeSymbol);

    if (typeIt == program->typeDeclMap.end() || typeIt->second == nullptr) {
      Error::internal(expr->value->span,
                      "failed to find enum HIR type declaration");
    }

    auto variantIt = program->variantMap.find(expr->variant);

    if (variantIt == program->variantMap.end() ||
        variantIt->second == nullptr) {
      Error::internal(expr->value->span, "failed to find HIR enum variant");
    }

    if (binding == nullptr) {
      Error::internal(expr->span, "payload case binding local is nullptr");
    }

    HIRPayloadCase payload{variantIt->second, binding};

    return make_unique<HIRCasePattern>(expr->span, payload);
  }

  Error::internal(expr->span, "unsupported case value AST kind");
}

unique_ptr<HIRStmt> HIRBuilder::lowerWhile(WhileStmt *stmt) {
  if (stmt == nullptr) {
    Error::internal("while statement is nullptr");
  }

  if (stmt->condition == nullptr) {
    Error::internal(stmt->span, "while condition is nullptr");
  }

  if (stmt->body == nullptr) {
    Error::internal(stmt->span, "while body is nullptr");
  }

  auto condition = lowerExpr(stmt->condition.get());
  auto body = lowerStmtAsBlock(stmt->body.get());

  if (condition == nullptr) {
    Error::internal(stmt->condition->span,
                    "while condition lowering returned nullptr");
  }

  if (body == nullptr) {
    Error::internal(stmt->body->span, "while body lowering returned nullptr");
  }

  return make_unique<HIRWhileStmt>(stmt->span, std::move(condition),
                                   std::move(body));
}

unique_ptr<HIRStmt> HIRBuilder::lowerReturn(ReturnStmt *stmt) {
  if (stmt == nullptr) {
    Error::internal("return statement is nullptr");
  }

  unique_ptr<HIRExpr> value = nullptr;

  if (stmt->value != nullptr) {
    value = lowerExpr(stmt->value.get());

    if (value == nullptr) {
      Error::internal(stmt->value->span,
                      "return value lowering returned nullptr");
    }
  }

  return make_unique<HIRReturnStmt>(stmt->span, std::move(value));
}

unique_ptr<HIRStmt> HIRBuilder::lowerSwitch(SwitchStmt *stmt) {
  if (stmt == nullptr) {
    Error::internal("switch statement is nullptr");
  }

  if (stmt->value == nullptr) {
    Error::internal(stmt->span, "switch condition is nullptr");
  }

  auto condition = lowerValue(stmt->value.get());

  if (condition == nullptr) {
    Error::internal(stmt->value->span,
                    "switch condition lowering returned nullptr");
  }

  vector<unique_ptr<HIRCase>> cases;
  cases.reserve(stmt->clauses.size());

  for (auto &clause : stmt->clauses) {
    if (clause == nullptr) {
      Error::internal(stmt->span, "switch case is nullptr");
    }

    auto loweredCase = lowerCase(clause.get());

    if (loweredCase == nullptr) {
      Error::internal(clause->span, "switch case lowering returned nullptr");
    }

    cases.push_back(std::move(loweredCase));
  }

  return make_unique<HIRSwitchStmt>(stmt->span, std::move(condition),
                                    std::move(cases));
}

unique_ptr<HIRStmt> HIRBuilder::lowerValueTransfer(ValueTransferStmt *stmt) {
  if (stmt == nullptr) {
    Error::internal("value transfer statement is nullptr");
  }

  if (stmt->value == nullptr) {
    Error::internal(stmt->span, "value transfer expression is nullptr");
  }

  auto value = lowerValue(stmt->value.get());

  if (value == nullptr) {
    Error::internal(stmt->value->span,
                    "value transfer lowering returned nullptr");
  }

  return make_unique<HIRValueTransferStmt>(stmt->span, std::move(value));
}

unique_ptr<HIRStmt> HIRBuilder::lowerExprStmt(ExprStmt *stmt) {
  if (stmt == nullptr) {
    Error::internal("expression statement is nullptr");
  }

  if (stmt->expr == nullptr) {
    Error::internal(stmt->span, "expression statement expression is nullptr");
  }

  if (auto *destroy = dynamic_cast<DestroyExpr *>(stmt->expr.get())) {
    return lowerDestroyStmt(destroy);
  }

  if (auto *quit = dynamic_cast<QuitExpr *>(stmt->expr.get())) {
    return lowerQuitStmt(quit);
  }

  if (auto *assign = dynamic_cast<AssignExpr *>(stmt->expr.get())) {
    return lowerAssign(assign);
  }

  auto expr = lowerExpr(stmt->expr.get());

  if (expr == nullptr) {
    Error::internal(stmt->expr->span,
                    "expression statement lowering returned nullptr");
  }

  return make_unique<HIRExprStmt>(stmt->span, std::move(expr));
}

unique_ptr<HIRStmt> HIRBuilder::lowerDestroyStmt(DestroyExpr *expr) {
  if (expr == nullptr) {
    Error::internal("destroy expression is nullptr");
  }

  if (expr->storage == nullptr) {
    Error::internal(expr->span, "destroy storage expression is nullptr");
  }

  if (expr->target == nullptr) {
    Error::internal(expr->span, "destroy target expression is nullptr");
  }

  auto *storage = dynamic_cast<BuiltInNameExpr *>(expr->storage.get());

  if (storage == nullptr) {
    Error::internal(expr->storage->span,
                    "destroy storage is not BuiltInNameExpr");
  }

  StorageKind storageKind;

  switch (storage->storageType) {
  case BuiltInNameExpr::StorageType::WORLD:
    storageKind = StorageKind::World;
    break;

  default:
    Error::internal(expr->storage->span, "unsupported destroy storage kind");
  }

  auto *name = dynamic_cast<NameExpr *>(expr->target.get());

  if (name == nullptr) {
    Error::internal(expr->target->span, "destroy target is not NameExpr");
  }

  auto place = lowerPlace(name);

  if (place == nullptr) {
    Error::internal(expr->target->span,
                    "destroy target lowering returned nullptr");
  }

  auto *handleType = dynamic_cast<HIRHandleType *>(place->type);

  if (handleType == nullptr) {
    Error::internal(expr->target->span,
                    "destroy target did not lower to Handle place");
  }

  if (handleType->storage != storageKind) {
    Error::internal(expr->target->span, "destroy handle storage kind mismatch");
  }

  if (handleType->entityType == nullptr) {
    Error::internal(expr->target->span,
                    "destroy handle entity type is nullptr");
  }

  auto handle = load(std::move(place));

  if (handle == nullptr) {
    Error::internal(expr->target->span, "destroy handle load returned nullptr");
  }

  return make_unique<HIRDestroyStmt>(expr->span, std::move(handle),
                                     handleType->entityType, storageKind);
}

unique_ptr<HIRStmt> HIRBuilder::lowerQuitStmt(QuitExpr *expr) {
  if (expr == nullptr) {
    Error::internal("quit expression is nullptr");
  }

  return make_unique<HIRQuitStmt>(expr->span);
}

unique_ptr<HIRStmt> HIRBuilder::lowerAssign(AssignExpr *expr) {
  if (expr == nullptr) {
    Error::internal("assignment expression is nullptr");
  }

  if (expr->target == nullptr) {
    Error::internal(expr->span, "assignment target is nullptr");
  }

  if (expr->value == nullptr) {
    Error::internal(expr->span, "assignment value is nullptr");
  }

  unique_ptr<HIRPlaceExpr> lhs = nullptr;

  if (auto *name = dynamic_cast<NameExpr *>(expr->target.get())) {
    lhs = lowerPlace(name);
  } else if (auto *member = dynamic_cast<MemberExpr *>(expr->target.get())) {
    lhs = lowerMember(member);
  } else if (auto *array =
                 dynamic_cast<ArrayAccessExpr *>(expr->target.get())) {
    lhs = lowerArrayAccess(array);
  } else {
    Error::internal(expr->target->span,
                    "assignment target is not a place expression");
  }

  if (lhs == nullptr) {
    Error::internal(expr->target->span,
                    "assignment target lowering returned nullptr");
  }

  if (auto *local = dynamic_cast<HIRLocalPlaceExpr *>(lhs.get())) {
    if (local->local == nullptr) {
      Error::internal(expr->target->span, "assignment local is nullptr");
    }

    if (!local->local->isMutable) {
      if (local->local->isCaseValue) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_H011);
        dia.labels = {
            {expr->target->span, "this payload binding cannot be reassigned",
             true},
        };
        dia.notes = {
            "payload bindings are immutable",
        };
        dia.helps = {
            "copy the payload value into a mutable local variable",
        };
        engine.emit(dia);
        recover.recover();
      }

      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_H012);
      dia.labels = {
          {expr->target->span, "this local value is immutable", true},
      };
      dia.notes = {
          "an immutable local can only be initialized once",
      };
      dia.helps = {
          "declare the local as mutable before assigning to it",
      };
      engine.emit(dia);
      recover.recover();
    }
  }

  if (auto *field = dynamic_cast<HIRFieldPlaceExpr *>(lhs.get())) {
    if (field->field == nullptr) {
      Error::internal(expr->target->span, "assignment field is nullptr");
    }

    if (!field->field->isMutable) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_H012);
      dia.labels = {
          {expr->target->span, "this field is immutable", true},
      };
      dia.notes = {
          "an immutable field can only be initialized once",
      };
      dia.helps = {
          "declare the field as mutable before assigning to it",
      };
      engine.emit(dia);
      recover.recover();
    }
  }

  if (auto *param = dynamic_cast<HIRParamPlaceExpr *>(lhs.get())) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_H013);
    dia.labels = {
        {param->span, "this parameter cannot be reassigned", true},
    };
    dia.notes = {
        "method parameters are immutable",
    };
    dia.helps = {
        "copy the parameter into a mutable local variable",
    };
    engine.emit(dia);
    recover.recover();
  }

  auto rhs = lowerValue(expr->value.get());

  if (rhs == nullptr) {
    Error::internal(expr->value->span,
                    "assignment value lowering returned nullptr");
  }

  switch (expr->op.kind) {
  case TKind::PLUS_EQUAL:
    return make_unique<HIRCompoundAssignStmt>(expr->span, std::move(lhs),
                                              std::move(rhs), Operator::ADD);

  case TKind::MINUS_EQUAL:
    return make_unique<HIRCompoundAssignStmt>(expr->span, std::move(lhs),
                                              std::move(rhs), Operator::SUB);

  case TKind::STAR_EQUAL:
    return make_unique<HIRCompoundAssignStmt>(expr->span, std::move(lhs),
                                              std::move(rhs), Operator::MUL);

  case TKind::DOUBLE_STAR_EQUAL:
    return make_unique<HIRCompoundAssignStmt>(expr->span, std::move(lhs),
                                              std::move(rhs), Operator::POW);

  case TKind::SLASH_EQUAL:
    return make_unique<HIRCompoundAssignStmt>(expr->span, std::move(lhs),
                                              std::move(rhs), Operator::DIV);

  case TKind::PERCENT_EQUAL:
    return make_unique<HIRCompoundAssignStmt>(expr->span, std::move(lhs),
                                              std::move(rhs), Operator::REM);

  case TKind::CARET_EQUAL:
    return make_unique<HIRCompoundAssignStmt>(expr->span, std::move(lhs),
                                              std::move(rhs), Operator::B_XOR);

  case TKind::AMPERSAND_EQAUL:
    return make_unique<HIRCompoundAssignStmt>(expr->span, std::move(lhs),
                                              std::move(rhs), Operator::B_AND);

  case TKind::PIPE_EQUAL:
    return make_unique<HIRCompoundAssignStmt>(expr->span, std::move(lhs),
                                              std::move(rhs), Operator::B_OR);

  case TKind::DOUBLE_ANGLEBUCKET_EQAUL:
    return make_unique<HIRCompoundAssignStmt>(expr->span, std::move(lhs),
                                              std::move(rhs), Operator::LSH);

  case TKind::DOUBLE_RIGHT_ANGLE_BUCKET_EQUAL:
    return make_unique<HIRCompoundAssignStmt>(expr->span, std::move(lhs),
                                              std::move(rhs), Operator::RSH);

  case TKind::EQUAL:
    return make_unique<HIRAssignStmt>(expr->span, std::move(lhs),
                                      std::move(rhs));

  default:
    Error::internal(expr->span, "unsupported assignment operator kind");
  }
}