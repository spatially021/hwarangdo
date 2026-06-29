
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

  unique_ptr<HIRBlockStmt> block = make_unique<HIRBlockStmt>(stmt->span);
  BlockGuard _(currentBlock, block.get());
  for (auto &s : stmt->statements) {
    s->accept(this);
  }
  return block;
}

unique_ptr<HIRBlockStmt> HIRBuilder::lowerStmtAsBlock(Stmt *stmt) {
  if (auto b = dynamic_cast<BlockStmt *>(stmt)) {
    return lowerBlock(b);
  }

  unique_ptr<HIRBlockStmt> block = make_unique<HIRBlockStmt>(stmt->span);
  BlockGuard _(currentBlock, block.get());
  stmt->accept(this);
  return block;
}

unique_ptr<HIRStmt> HIRBuilder::lowerFor(ForStmt *stmt) {
  HIRLocal *local = nullptr;

  if (auto d = dynamic_cast<DeclStmt *>(stmt->initializer.get())) {
    if (auto v = dynamic_cast<VarDecl *>(d->decl.get())) {
      local = lowerLocal(v);
    } else {
      Error::internal(stmt->span, "for initializer decl is not VarDecl");
    }
  } else {
    Error::internal(stmt->span, "for initializer is not DeclStmt");
  }

  auto from = lowerExpr(stmt->range->from.get());
  auto to = lowerExpr(stmt->range->to.get());
  auto step = lowerExpr(stmt->range->step.get());
  auto body = lowerStmtAsBlock(stmt->body.get());

  return make_unique<HIRForRangeStmt>(stmt->span, local, std::move(from),
                                      std::move(to), std::move(step),
                                      std::move(body));
}
unique_ptr<HIRStmt> HIRBuilder::lowerIf(IfStmt *stmt) {
  unique_ptr<HIRExpr> temp = lowerExpr(stmt->condition.get());
  unique_ptr<HIRValueExpr> cond;
  if (dynamic_cast<HIRValueExpr *>(temp.get())) {
    cond =
        unique_ptr<HIRValueExpr>(static_cast<HIRValueExpr *>(temp.release()));
  } else if (dynamic_cast<HIRPlaceExpr *>(temp.get())) {
    cond = load(
        unique_ptr<HIRPlaceExpr>(static_cast<HIRPlaceExpr *>(temp.release())));
  } else {
    Error::internal(stmt->condition->span, "ifStmt cond is not place or value");
  }

  unique_ptr<HIRBlockStmt> thenBlock = lowerStmtAsBlock(stmt->thenBranch.get());
  unique_ptr<HIRBlockStmt> elseBlock =
      stmt->elseBranch ? lowerStmtAsBlock(stmt->elseBranch.get()) : nullptr;
  return make_unique<HIRIfStmt>(stmt->span, std::move(cond),
                                std::move(thenBlock), std::move(elseBlock));
}

unique_ptr<HIRCase> HIRBuilder::lowerCase(Case *stmt) {

  vector<unique_ptr<HIRCasePattern>> selectors;
  for (auto &s : stmt->values) {
    auto caseValue = dynamic_cast<CaseValueExpr *>(s.get());
    if (caseValue == nullptr) {
      Error::internal(s->span, "illegal ast kind");
    }

    selectors.push_back(lowerCaseValue(caseValue));
  }

  HIRDefaultKind dKind = HIRDefaultKind::None;
  if (stmt->isDefault) {
    dKind = HIRDefaultKind::Default;

  } else if (stmt->isWildCard) {
    dKind = HIRDefaultKind::WildCard;
  }
  unique_ptr<HIRBlockStmt> body = lowerStmtAsBlock(stmt->body.get());
  return make_unique<HIRCase>(stmt->span, std::move(selectors), std::move(body),
                              dKind);
}

unique_ptr<HIRCasePattern> HIRBuilder::lowerCaseValue(CaseValueExpr *expr) {
  if (expr->isWildCard) {
    return make_unique<HIRCasePattern>(expr->span, HIRWildcardCase());
  }

  if (auto lit = dynamic_cast<LiteralExpr *>(expr->value.get())) {
    auto literal = lowerLiteral(lit);

    auto *raw = literal.get();
    if (auto *literalExpr = dynamic_cast<HIRLiteralExpr *>(raw)) {
      auto *released = literal.release();
      (void)released;

      auto literalPattern =
          HIRLiteralCase(std::unique_ptr<HIRLiteralExpr>(literalExpr));

      return make_unique<HIRCasePattern>(expr->span, std::move(literalPattern));
    }

    Error::internal(lit->span, "fail to cast literalExpr");
  }

  if (expr->variant == nullptr) {
    Error::internal(expr->value->span,
                    "case value is enum-variant but variant is nullptr");
  }

  if (dynamic_cast<NameExpr *>(expr->value.get())) {
    auto it = program->typeDeclMap.find(expr->variant->typeSymbol);
    if (it == program->typeDeclMap.end()) {
      Error::internal(expr->value->span, "fail to get enum-variant's type");
    }
    auto vIt = program->variantMap.find(expr->variant);

    if (vIt == program->variantMap.end()) {
      Error::internal(expr->value->span, "fail to get enum variant");
    }

    auto unit = HIRUnitCase(vIt->second);
    return make_unique<HIRCasePattern>(expr->span, unit);
  }

  if (auto member = dynamic_cast<MemberExpr *>(expr->value.get())) {
    if (isTypeReceiver(member->object.get())) {
      auto type = member->object->resolvedType;
      auto it = program->typeDeclMap.find(type);
      if (it == program->typeDeclMap.end()) {
        Error::internal(expr->span, "fail to get typeDecl");
      }
      if (it->second->typeDeclKind != HIRTypeDeclKind::Enum) {
        Error::internal(expr->span, "illegal type kind");
      }

      auto viT = program->variantMap.find(expr->variant);
      if (viT == program->variantMap.end()) {
        Error::internal(expr->span, "fail to get variant");
      }

      auto unit = HIRUnitCase(viT->second);
      return make_unique<HIRCasePattern>(expr->span, unit);
    }

    Error::internal(expr->span, "illegal receiver type");
  }

  HIRLocal *raw = nullptr;

  if (expr->payload) {
    unique_ptr<HIRLocal> local = make_unique<HIRLocal>();
    local->id = allocLocalID();
    local->isInitialized = true;
    local->isMutable = false;
    local->name = expr->payload->name;
    local->symbol = expr->payload;
    auto type =
        HIRHelper::lowerType(program, source, expr->payload->typeSymbol);
    local->type = type;
    local->isCaseValue = true;
    raw = local.get();
    bindLocal(expr->payload, std::move(local));
  }

  if (dynamic_cast<CallExpr *>(expr->value.get())) {
    auto it = program->typeDeclMap.find(expr->variant->typeSymbol);
    if (it == program->typeDeclMap.end()) {
      Error::internal(expr->value->span, "fail to get enum-variant's type");
    }
    auto vIt = program->variantMap.find(expr->variant);

    if (vIt == program->variantMap.end()) {
      Error::internal(expr->value->span, "fail to get enum variant");
    }

    if (raw == nullptr) {
      Error::internal(expr->arg->span, "fail to make binding local");
    }

    auto payload = HIRPayloadCase(vIt->second, raw);
    return make_unique<HIRCasePattern>(expr->span, payload);
  }

  Error::internal(expr->span, "illegal ast kind");
}

unique_ptr<HIRStmt> HIRBuilder::lowerWhile(WhileStmt *stmt) {
  unique_ptr<HIRExpr> cond = lowerExpr(stmt->condition.get());
  unique_ptr<HIRBlockStmt> body = lowerStmtAsBlock(stmt->body.get());
  return (
      make_unique<HIRWhileStmt>(stmt->span, std::move(cond), std::move(body)));
}

unique_ptr<HIRStmt> HIRBuilder::lowerReturn(ReturnStmt *stmt) {
  std::unique_ptr<HIRExpr> expr = nullptr;
  if (stmt->value) {
    expr = lowerExpr(stmt->value.get());
  }
  return (make_unique<HIRReturnStmt>(stmt->span, std::move(expr)));
}

unique_ptr<HIRStmt> HIRBuilder::lowerSwitch(SwitchStmt *stmt) {
  unique_ptr<HIRValueExpr> cond = lowerValue(stmt->value.get());
  vector<unique_ptr<HIRCase>> cases;
  for (auto &c : stmt->clauses) {
    cases.push_back(lowerCase(c.get()));
  }
  return (make_unique<HIRSwitchStmt>(stmt->span, std::move(cond),
                                     std::move(cases)));
}

unique_ptr<HIRStmt> HIRBuilder::lowerValueTransfer(ValueTransferStmt *stmt) {
  unique_ptr<HIRValueExpr> value = lowerValue(stmt->value.get());
  return make_unique<HIRValueTransferStmt>(stmt->span, std::move(value));
}

unique_ptr<HIRStmt> HIRBuilder::lowerExprStmt(ExprStmt *stmt) {

  if (auto destroy = dynamic_cast<DestroyExpr *>(stmt->expr.get())) {
    return lowerDestroyStmt(destroy);
  }
  if (auto quit = dynamic_cast<QuitExpr *>(stmt->expr.get())) {
    return lowerQuitStmt(quit);
  }
  if (auto assign = dynamic_cast<AssignExpr *>(stmt->expr.get())) {
    return lowerAssign(assign);
  }

  return make_unique<HIRExprStmt>(stmt->span, lowerExpr(stmt->expr.get()));
}

unique_ptr<HIRStmt> HIRBuilder::lowerDestroyStmt(DestroyExpr *expr) {
  auto storage = dynamic_cast<BuiltInNameExpr *>(expr->storage.get());
  if (storage == nullptr) {
    Error::internal(expr->span, "iliegal astNode kind");
  }
  StorageKind storageKind;
  switch (storage->storageType) {
  case BuiltInNameExpr::StorageType::WORLD:
    storageKind = StorageKind::World;
    break;
  case BuiltInNameExpr::StorageType::ARENA:
    storageKind = StorageKind::Arena;
    break;
  default:
    Error::internal(expr->span, "unknown storage kind");
  };

  NameExpr *name = dynamic_cast<NameExpr *>(expr->target.get());

  if (name == nullptr) {
    Error::internal(expr->span, "illegal astNode type");
  }

  auto place = lowerPlace(name);

  auto handleType = dynamic_cast<HIRHandleType *>(place->type);
  if (handleType == nullptr) {
    Error::internal(expr->span, "expect handle : " + place->type->name);
  }

  if (handleType->storage != storageKind) {
    Error::internal(expr->span, "handle storage kind mismatch");
  }

  HIREntityType *entity = handleType->entityType;

  unique_ptr<HIRValueExpr> handle = load(std::move(place));

  return make_unique<HIRDestroyStmt>(expr->span, std::move(handle), entity,
                                     storageKind);
}

unique_ptr<HIRStmt> HIRBuilder::lowerQuitStmt(QuitExpr *expr) {
  return make_unique<HIRQuitStmt>(expr->span);
}

unique_ptr<HIRStmt> HIRBuilder::lowerAssign(AssignExpr *expr) {
  unique_ptr<HIRPlaceExpr> lhs = nullptr;
  if (auto name = dynamic_cast<NameExpr *>(expr->target.get())) {
    // nameExpr -> place
    lhs = lowerPlace(name);
  } else if (auto member = dynamic_cast<MemberExpr *>(expr->target.get())) {
    // memberExpr -> fieldplace
    lhs = lowerMember(member);
  } else if (auto array = dynamic_cast<ArrayAccessExpr *>(expr->target.get())) {
    lhs = lowerArrayAccess(array);
  } else {
    Error::internal(expr->span, "lhs is not nameExpr");
  }

  if (auto local = dynamic_cast<HIRLocalPlaceExpr *>(lhs.get())) {
    if (!local->local->isMutable) {
      if (local->local->isCaseValue) {
        Error::diagnostic(expr->target->span,
                          " cannot assign to payload binding ");
      } else {
        Error::diagnostic(expr->target->span, "const value cannot reallocate");
      }
    }
  }

  if (auto field = dynamic_cast<HIRFieldPlaceExpr *>(lhs.get())) {
    if (!field->field->isMutable) {
      Error::diagnostic(expr->target->span, "const value cannot reallocate");
    }
  }

  if (auto param = dynamic_cast<HIRParamPlaceExpr *>(lhs.get())) {
    Error::diagnostic(param->span, "param value cannot reallocate");
  }

  unique_ptr<HIRValueExpr> rhs = lowerValue(expr->value.get());

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
                                              std::move(rhs), Operator::SUB);

  case TKind::PERCENT_EQUAL:
    return make_unique<HIRCompoundAssignStmt>(expr->span, std::move(lhs),
                                              std::move(rhs), Operator::REM);

  case TKind::CARET_EQUAL:
    return make_unique<HIRCompoundAssignStmt>(expr->span, std::move(lhs),
                                              std::move(rhs), Operator::B_AND);

  case TKind::AMPERSAND_EQAUL:
    return make_unique<HIRCompoundAssignStmt>(expr->span, std::move(lhs),
                                              std::move(rhs), Operator::B_XOR);

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
    Error::internal(expr->span, "illegal operator kind");
    break;
  }

  // expr->hirvalueExpr
  // not allowed nullptr
}