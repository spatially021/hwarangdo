
#include "hrd/IR/HIR/HIRExpr.h"
#include "hrd/IR/HIR/HIRNode.h"
#include "hrd/IR/HIR/HIRSymbol.h"
#include "hrd/IR/MIR/MIRBuilder.h"
#include "hrd/IR/MIR/MIRExpr.h"
#include "hrd/IR/MIR/MIRNode.h"
#include "hrd/IR/MIR/MIRStmt.h"
#include "hrd/util/Error.h"
#include <memory>
#include <utility>
#include <vector>

unique_ptr<MIRValue> MIRBuilder::lowerExpr(HIRExpr *expr) {
  switch (expr->kind) {

  case HIRNodeKind::LiteralExpr: {
    auto lit = expect<HIRLiteralExpr>(expr, HIRNodeKind::LiteralExpr);
    return lowerLiteral(lit);
  }
  case HIRNodeKind::LoadExpr: {
    auto load = expect<HIRLoadExpr>(expr, HIRNodeKind::LoadExpr);
    return lowerLoad(load);
  }
  case HIRNodeKind::UnaryExpr: {
    auto unary = expect<HIRUnaryExpr>(expr, HIRNodeKind::UnaryExpr);
    return lowerUnary(unary);
    break;
  }
  case HIRNodeKind::BinaryExpr: {
    auto binary = expect<HIRBinaryExpr>(expr, HIRNodeKind::BinaryExpr);
    return lowerBinary(binary);
  }
  case HIRNodeKind::CastExpr: {
    auto cast = expect<HIRCastExpr>(expr, HIRNodeKind::CastExpr);
    return lowerCast(cast);
  }
  case HIRNodeKind::MethodCallExpr: {
    auto call = expect<HIRMethodCallExpr>(expr, HIRNodeKind::MethodCallExpr);
    return lowerCall(call);
  }

  case HIRNodeKind::RuntimeCallExpr: {
    auto runtime = expect<HIRRuntimeCall>(expr, HIRNodeKind::RuntimeCallExpr);
    return lowerRuntime(runtime);
  }

  case HIRNodeKind::SpawnExpr: {
    auto spawn = expect<HIRSpawnExpr>(expr, HIRNodeKind::SpawnExpr);
    return lowerSpawn(spawn);
  }
  case HIRNodeKind::ViewExpr: {
    auto view = expect<HIRViewExpr>(expr, HIRNodeKind::ViewExpr);
    return lowerView(view);
  }
  case HIRNodeKind::MatchExpr: {
    auto match = expect<HIRMatchExpr>(expr, HIRNodeKind::MatchExpr);
    return lowerMatch(match);
  }
  case HIRNodeKind::TernaryExpr: {
    auto tern = expect<HIRTernaryExpr>(expr, HIRNodeKind::TernaryExpr);
    return lowerTernary(tern);
  }
  case HIRNodeKind::EnumVariantValue: {
    auto variant =
        expect<HIRVariantValueExpr>(expr, HIRNodeKind::EnumVariantValue);
    return lowerVariantValue(variant);
  }
  case HIRNodeKind::StructInitExpr: {
    auto init = expect<HIRStructInitExpr>(expr, HIRNodeKind::StructInitExpr);
    return lowerStructInit(init);
  }

  case HIRNodeKind::SelfExpr: {
    auto self = expect<HIRSelfExpr>(expr, HIRNodeKind::SelfExpr);
    return lowerSelf(self);
  }

  default: {
    break;
  }
  }

  Error::internal("ilegal hir kind");
}
unique_ptr<MIRValue> MIRBuilder::lowerTernary(HIRTernaryExpr *expr) {
  auto temp = makeTemp(expr->type->typeSymbol);

  BlockID entry = currentBlock;
  BlockID cond = makeBlock();
  BlockID then = makeBlock();
  BlockID else_ = makeBlock();
  BlockID join = makeBlock();

  auto type = expr->type->typeSymbol;

  emit(make_unique<MIRLocalDeclStmt>(type, temp, nullptr));

  getBlock(entry)->terminator = GotoTerminator(cond);

  currentBlock = cond;
  auto condExpr = lowerExpr(expr->condition.get());
  getBlock(cond)->terminator =
      BranchTerminator(std::move(condExpr), then, else_);

  currentBlock = then;
  emit(make_unique<MIRAssignStmt>(make_unique<MIRLocalPlace>(temp),
                                  lowerExpr(expr->thenExpr.get())));
  getBlock(then)->terminator = GotoTerminator(join);

  currentBlock = else_;
  emit(make_unique<MIRAssignStmt>(make_unique<MIRLocalPlace>(temp),
                                  lowerExpr(expr->elseExpr.get())));
  getBlock(else_)->terminator = GotoTerminator(join);

  currentBlock = join;
  return make_unique<MIRLoad>(make_unique<MIRLocalPlace>(temp),
                              expr->type->typeSymbol);
}

unique_ptr<MIRValue> MIRBuilder::lowerMatch(HIRMatchExpr *expr) {
  BlockID entry = currentBlock;
  BlockID cond = makeBlock();
  BlockID defaultTarget = makeBlock();
  BlockID join = makeBlock();
  vector<MIRCase> cases;

  bool hasDefault = false;

  auto temp = makeTemp(expr->type->typeSymbol);
  emit(make_unique<MIRLocalDeclStmt>(temp->typeSymbol, temp, nullptr));
  getBlock(entry)->terminator = GotoTerminator(cond);

  MatchContext m = {temp, join};
  matches.push_back(m);

  unique_ptr<MIRValue> condExpr = lowerExpr(expr->cond.get());
  auto tempCond = makeTemp(expr->cond->type->typeSymbol);

  emit(make_unique<MIRLocalDeclStmt>(tempCond->typeSymbol, tempCond,
                                     std::move(condExpr)));

  for (auto &c : expr->cases) {
    assert(c->defaultKind != HIRDefaultKind::Default);
    BlockID id;
    if (c->defaultKind == HIRDefaultKind::WildCard) {
      id = defaultTarget;
      hasDefault = true;
    } else {
      id = makeBlock();
    }
    currentBlock = id;
    for (auto &v : c->selectors) {
      auto s = &v->selector;
      if (auto lit = std::get_if<HIRLiteralCase>(s)) {
        cases.push_back(MIRCase(lit->expr->resolvedLit, id));
        continue;
      }
      if (auto unit = std::get_if<HIRUnitCase>(s)) {
        cases.push_back(MIRCase(unit->variant->symbol, id));
        continue;
      }
      if (auto payload = std::get_if<HIRPayloadCase>(s)) {

        emit(make_unique<MIRLocalDeclStmt>(
            payload->binding->type->typeSymbol, payload->binding->symbol,
            make_unique<MIRPayloadExtractExpr>(
                payload->variant->symbol,
                make_unique<MIRLoad>(make_unique<MIRLocalPlace>(tempCond),
                                     expr->type->typeSymbol),
                expr->type->typeSymbol)));
        cases.push_back(MIRCase(payload->variant->symbol, id));
      }
    }
    lowerBlock(c->body.get());
    if (!hasTerminator(currentBlock)) {
      getBlock(currentBlock)->terminator = GotoTerminator(join);
    }
  }

  if (hasDefault && !hasTerminator(defaultTarget)) {
    getBlock(defaultTarget)->terminator = GotoTerminator(join);
  }

  getBlock(cond)->terminator = SwitchTerminator(
      make_unique<MIRLoad>(make_unique<MIRLocalPlace>(tempCond),
                           expr->type->typeSymbol),
      std::move(cases), hasDefault ? defaultTarget : join);
  currentBlock = join;

  matches.pop_back();

  return make_unique<MIRLoad>(make_unique<MIRLocalPlace>(temp),
                              expr->type->typeSymbol);
}

unique_ptr<MIRValue> MIRBuilder::lowerLiteral(HIRLiteralExpr *expr) {
  return make_unique<MIRLiteralExpr>(expr->resolvedLit, expr->type->typeSymbol);
}

unique_ptr<MIRValue> MIRBuilder::lowerLoad(HIRLoadExpr *expr) {
  return make_unique<MIRLoad>(lowerPlace(expr->place.get()),
                              expr->type->typeSymbol);
}

unique_ptr<MIRValue> MIRBuilder::lowerUnary(HIRUnaryExpr *expr) {
  return make_unique<MIRUnaryExpr>(lowerExpr(expr->operand.get()), expr->op,
                                   expr->type->typeSymbol);
}

unique_ptr<MIRValue> MIRBuilder::lowerBinary(HIRBinaryExpr *expr) {
  return make_unique<MIRBinaryExpr>(lowerExpr(expr->left.get()),
                                    lowerExpr(expr->right.get()), expr->op,
                                    expr->type->typeSymbol, expr->operand);
}

unique_ptr<MIRValue> MIRBuilder::lowerCast(HIRCastExpr *expr) {
  return make_unique<MIRCastExpr>(lowerExpr(expr->operand.get()),
                                  expr->fromType->typeSymbol,
                                  expr->toType->typeSymbol);
}

unique_ptr<MIRValue> MIRBuilder::lowerCall(HIRMethodCallExpr *expr) {
  vector<unique_ptr<MIRValue>> args;
  for (auto &a : expr->args) {
    args.push_back(lowerExpr(a.get()));
  }

  return make_unique<MIRCallExpr>(lowerExpr(expr->receiver.get()),
                                  std::move(args), expr->method->symbol,
                                  expr->type->typeSymbol);
}

unique_ptr<MIRValue> MIRBuilder::lowerSpawn(HIRSpawnExpr *expr) {
  std::vector<unique_ptr<MIRValue>> args;
  for (auto &a : expr->args) {
    args.push_back(lowerExpr(a.get()));
  }
  return make_unique<MIRSpawnExpr>(expr->entityType->typeSymbol,
                                   expr->initMethod ? expr->initMethod->symbol
                                                    : nullptr,
                                   std::move(args), expr->type->typeSymbol);
}

unique_ptr<MIRValue> MIRBuilder::lowerView(HIRViewExpr *expr) {
  return make_unique<MIRViewExpr>(expr->entityType->typeSymbol,
                                  lowerExpr(expr->handle.get()),
                                  expr->type->typeSymbol);
}

unique_ptr<MIRValue> MIRBuilder::lowerStructInit(HIRStructInitExpr *expr) {
  std::vector<unique_ptr<MIRValue>> args;
  for (auto &a : expr->args) {
    args.push_back(lowerExpr(a.get()));
  }
  return make_unique<MIRStructInitExpr>(
      expr->type->typeSymbol, expr->method ? expr->method->symbol : nullptr,
      std::move(args), expr->type->typeSymbol);
}

unique_ptr<MIRValue> MIRBuilder::lowerVariantValue(HIRVariantValueExpr *expr) {
  if (expr->payload) {
    return make_unique<MIRVariantExpr>(expr->varaint->symbol,
                                       lowerExpr(expr->payload.get()),
                                       expr->type->typeSymbol);
  }
  return make_unique<MIRVariantExpr>(expr->varaint->symbol, nullptr,
                                     expr->type->typeSymbol);
}

unique_ptr<MIRValue> MIRBuilder::lowerSelf(HIRSelfExpr *expr) {
  return make_unique<MIRLoad>(
      make_unique<MIRParamPlace>(currentFunc->symbol->selfReceiver),
      expr->selfKind == HIRSelfKind::Super ? expr->accessType->typeSymbol
                                           : expr->ownerType->typeSymbol);
}

unique_ptr<MIRValue> MIRBuilder::lowerRuntime(HIRRuntimeCall *expr) {
  vector<unique_ptr<MIRValue>> args;
  for (auto &a : expr->args) {
    args.push_back(lowerExpr(a.get()));
  }
  return make_unique<MIRRuntimeCallExpr>(expr->symbol, std::move(args),
                                         expr->type->typeSymbol);
}
