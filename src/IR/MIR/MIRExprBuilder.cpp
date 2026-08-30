
#include "hrd/IR/HIR/HIRExpr.h"
#include "hrd/IR/HIR/HIRNode.h"
#include "hrd/IR/HIR/HIRSymbol.h"
#include "hrd/IR/MIR/MIRBuilder.h"
#include "hrd/IR/MIR/MIRExpr.h"
#include "hrd/IR/MIR/MIRNode.h"
#include "hrd/IR/MIR/MIRStmt.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
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
  case HIRNodeKind::ArrayLiteralExpr: {
    auto arr = expect<HIRArrayLiteralExpr>(expr, HIRNodeKind::ArrayLiteralExpr);
    return lowerArrayLiteral(arr);
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

  Error::internal("illegal hir kind");
}
unique_ptr<MIRValue> MIRBuilder::lowerTernary(HIRTernaryExpr *expr) {
  auto temp = makeTemp(expr->type);

  BlockID entry = currentBlock;
  BlockID cond = makeBlock();
  BlockID then = makeBlock();
  BlockID else_ = makeBlock();
  BlockID join = makeBlock();

  auto type = expr->type;

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
  return make_unique<MIRLoad>(make_unique<MIRLocalPlace>(temp), expr->type);
}

unique_ptr<MIRValue> MIRBuilder::lowerMatch(HIRMatchExpr *expr) {
  BlockID entry = currentBlock;
  IRScope switchScope(currentScope, currentScope->depth + 1);

  auto *outerScope = currentScope;
  currentScope = &switchScope;

  BlockID cond = makeBlock();
  BlockID defaultTarget = makeBlock();
  BlockID cleanup = makeBlock();
  BlockID join = makeBlock();

  currentBlock = entry;

  auto result = makeTemp(expr->type);
  outerScope->locals.push_back(result);
  emit(make_unique<MIRLocalDeclStmt>(result->typeSymbol, result, nullptr));
  getBlock(entry)->terminator = GotoTerminator(cond);

  MatchContext m = {result, cleanup};
  matches.push_back(m);
  currentBlock = cond;
  SwitchData data = {cond,        defaultTarget,   cleanup,
                     join,        switchScope,     expr->cond.get(),
                     expr->cases, expr->cond->type};

  makeSwitch(data);

  if (!hasTerminator(cleanup)) {
    getBlock(cleanup)->terminator = GotoTerminator(join);
  }

  currentScope = outerScope;
  currentBlock = join;
  matches.pop_back();

  auto load =
      make_unique<MIRLoad>(make_unique<MIRLocalPlace>(result), expr->type);
  load->valueCategory = MIRValueCategory::OwnedTemp;
  return load;
}

unique_ptr<MIRValue> MIRBuilder::lowerLiteral(HIRLiteralExpr *expr) {
  auto temp = make_unique<MIRLiteralExpr>(expr->resolvedLit, expr->type);
  if (isa<StringType>(expr->type)) {
    temp->valueCategory = MIRValueCategory::Borrowed;
  }
  return temp;
}

unique_ptr<MIRValue> MIRBuilder::lowerLoad(HIRLoadExpr *expr) {
  auto temp = make_unique<MIRLoad>(lowerPlace(expr->place.get()), expr->type);
  if (isa<StringType>(expr->type)) {
    temp->valueCategory = MIRValueCategory::Borrowed;
  }
  return temp;
}

unique_ptr<MIRValue> MIRBuilder::lowerUnary(HIRUnaryExpr *expr) {
  return make_unique<MIRUnaryExpr>(lowerExpr(expr->operand.get()), expr->op,
                                   expr->type);
}

unique_ptr<MIRValue> MIRBuilder::lowerBinary(HIRBinaryExpr *expr) {
  auto temp = make_unique<MIRBinaryExpr>(lowerExpr(expr->left.get()),
                                         lowerExpr(expr->right.get()), expr->op,
                                         expr->type, expr->operand);
  if (isa<StringType>(expr->type)) {
    temp->valueCategory = MIRValueCategory::OwnedTemp;
  }
  return temp;
}

unique_ptr<MIRValue> MIRBuilder::lowerCast(HIRCastExpr *expr) {
  return make_unique<MIRCastExpr>(lowerExpr(expr->operand.get()),
                                  expr->fromType, expr->toType);
}

unique_ptr<MIRValue> MIRBuilder::lowerCall(HIRMethodCallExpr *expr) {
  vector<unique_ptr<MIRValue>> args;
  for (auto &a : expr->args) {
    args.push_back(lowerExpr(a.get()));
  }

  return make_unique<MIRCallExpr>(lowerExpr(expr->receiver.get()),
                                  std::move(args), expr->method, expr->type);
}

unique_ptr<MIRValue> MIRBuilder::lowerSpawn(HIRSpawnExpr *expr) {
  std::vector<unique_ptr<MIRValue>> args;
  for (auto &a : expr->args) {
    args.push_back(lowerExpr(a.get()));
  }
  return make_unique<MIRSpawnExpr>(
      expr->entityType, expr->initMethod ? expr->initMethod->symbol : nullptr,
      std::move(args), expr->type);
}

unique_ptr<MIRValue> MIRBuilder::lowerView(HIRViewExpr *expr) {
  return make_unique<MIRViewExpr>(expr->entityType,
                                  lowerExpr(expr->handle.get()), expr->type);
}

unique_ptr<MIRValue> MIRBuilder::lowerStructInit(HIRStructInitExpr *expr) {
  std::vector<unique_ptr<MIRValue>> args;
  for (auto &a : expr->args) {
    args.push_back(lowerExpr(a.get()));
  }
  return make_unique<MIRStructInitExpr>(expr->type,
                                        expr->method ? expr->method : nullptr,
                                        std::move(args), expr->type);
}

unique_ptr<MIRValue> MIRBuilder::lowerVariantValue(HIRVariantValueExpr *expr) {
  if (expr->payload) {
    return make_unique<MIRVariantExpr>(
        expr->varaint, lowerExpr(expr->payload.get()), expr->type);
  }
  return make_unique<MIRVariantExpr>(expr->varaint, nullptr, expr->type);
}

unique_ptr<MIRValue> MIRBuilder::lowerSelf(HIRSelfExpr *expr) {
  return make_unique<MIRLoad>(
      make_unique<MIRParamPlace>(currentFunc->symbol->selfReceiver),
      expr->selfKind == HIRSelfKind::Super ? expr->accessType
                                           : expr->ownerType);
}

unique_ptr<MIRValue> MIRBuilder::lowerRuntime(HIRRuntimeCall *expr) {
  vector<unique_ptr<MIRValue>> args;
  for (auto &a : expr->args) {
    args.push_back(lowerExpr(a.get()));
  }
  return make_unique<MIRRuntimeCallExpr>(expr->symbol, std::move(args),
                                         expr->type);
}

unique_ptr<MIRValue> MIRBuilder::lowerArrayLiteral(HIRArrayLiteralExpr *expr) {
  vector<unique_ptr<MIRValue>> elements;

  elements.reserve(expr->elements.size());

  for (auto &e : expr->elements) {
    elements.push_back(lowerExpr(e.get()));
  }

  return make_unique<MIRArrayInitExpr>(expr->type, expr->elementType,
                                       std::move(elements));
}