#include "AST/ASTNode.h"
#include "AST/CaseKey.h"
#include "AST/Decl.h"
#include "AST/Expr.h"
#include "AST/Stmt.h"
#include "SemanticAnalyzer/ResolvedLit.h"
#include "SemanticAnalyzer/Resolver.h"
#include "SemanticAnalyzer/Scope.h"
#include "SemanticAnalyzer/symbol/MethodSymbol.h"
#include "SemanticAnalyzer/symbol/StorageSymbol.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "SemanticAnalyzer/symbol/ValueSymbol.h"
#include "Token.h"
#include "enums/Operator.h"
#include "util/Error.h"
#include "util/Guard.h"
#include "util/TypeResolver.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

void Resolver::visit(LiteralExpr *expr) {
  ResolvedLit r;
  switch (expr->token.kind) {
  case TKind::LIT_INT:
    r = TypeResolver::resolveLitInt(expr, table);
    expr->resolvedType = r.type;
    expr->resolvedLit = r;
    break;
  case TKind::LIT_FLOAT:
    r = resolveLitFloat(expr);
    expr->resolvedType = r.type;
    expr->resolvedLit = r;
    break;
  case TKind::FIXED:
    expr->resolvedType = table->getType("fi16");
    break;
  case TKind::LIT_CHARACTER:
    r = resolveChar(expr);
    expr->resolvedType = r.type;
    expr->resolvedLit = r;
    break;
  case TKind::LIT_STRING:
    r = resolveString(expr);
    expr->resolvedType = r.type;
    expr->resolvedLit = r;
    break;
  case TKind::LIT_BOOL:
    expr->resolvedType = table->getBool();
    r.type = table->getBool();
    r.value = expr->value == "true";
    expr->resolvedLit = r;
    break;
  default:
    Error::diagnostic(expr->token, "unknown literal type");
  }
}
void Resolver::visit(BinaryExpr *expr) {
  expr->left->accept(this);
  expr->right->accept(this);

  if (expr->left->resolvedType == nullptr) {
    Error::internal(expr->left->span, "lhs is nullptr");
  }

  if (expr->right->resolvedType == nullptr) {
    Error::internal(expr->right->span, "rhs is nullptr");
  }

  if (isBinaryOperatalbe(expr->op, expr->left->resolvedType,
                         expr->right->resolvedType)) {
    auto temp = binaryResult(expr->op, expr->left->resolvedType,
                             expr->right->resolvedType);
    if (!temp) {
      Error::internal(expr->span, "fail to get binaryResult");
    }
    expr->resolvedType = temp;
  } else {
    Error::diagnostic(expr->span, "leftExpr and rightExpr cannot operate. [ " +
                                      expr->left->resolvedType->name + " " +
                                      expr->opRaw.text + " " +
                                      expr->right->resolvedType->name + " ]");
  }

  if (!expr->resolvedType) {
    Error::internal(expr->span, "unresolved type");
  }
}
void Resolver::visit(NameExpr *expr) {
  auto *symbol = resolveValue(expr->name);

  if (!symbol) {
    auto *temp = table->getType(expr->name);
    if (temp) {

      if (temp->kind == TypeSymbol::TypeKind::ENUM) {
        expr->resolvedType = temp;
        expr->resolved = temp;
        return;
      }
      // TODO: 추후 static메서드 추가시 추가 적용 필요.
      Error::internal(expr->span, "expect enum");
    }

    if (currentType == nullptr) {
      Error::internal("currentType is nullptr");
    }

    if (currentType->baseName.has_value() && currentType->base) {
      symbol = lookLocalValue(expr->name, currentType->base->memberScope);
    }

    if (!symbol) {
      Error::diagnostic(expr->span, "undeclared variable: " + expr->name);
    }
  }

  expr->resolved = symbol;
  expr->resolvedType = symbol->typeSymbol;

  if (!expr->resolvedType) {
    Error::internal(expr->span, "type symbol is nullptr");
  }

  if (dynamic_cast<HandleSymbol *>(expr->resolvedType)) {
    Error::internal(expr->span, "handle gotten");
  }
}

void Resolver::visit(UnaryExpr *expr) {
  expr->right->accept(this);

  if (expr->tOp.kind == TKind::BANG) {
    if (table->isBool(expr->right->resolvedType)) {
      expr->resolvedType = expr->right->resolvedType;
      expr->op = Operator::L_NOT;
    } else if (table->isInt(expr->right->resolvedType)) {
      expr->resolvedType = expr->right->resolvedType;
      expr->op = Operator::B_NOT;
    } else
      Error::diagnostic(expr->span, "bad operand type " +
                                        expr->right->resolvedType->name +
                                        " for unary operator '!'");
  } else if (expr->tOp.kind == TKind::PLUS || expr->tOp.kind == TKind::MINUS) {
    if (table->isNumberic(expr->right->resolvedType)) {
      expr->resolvedType = expr->right->resolvedType;
    } else
      Error::diagnostic(expr->span,
                        "bad operand type " + expr->right->resolvedType->name +
                            " for unary operator '" + expr->tOp.text + "'");

    if (expr->tOp.kind == TKind::PLUS) {
      expr->op = Operator::PLUS;
    } else {
      expr->op = Operator::MINUS;
      if (!table->isSigned(expr->right->resolvedType)) {
        Error::diagnostic(expr->right->span, " '-' cannot use with unsigned");
      }
    }
  }
}

void Resolver::visit(AssignExpr *expr) {
  expr->target->accept(this);
  expr->value->accept(this);
  if (!isAssignable(expr->target->resolvedType, expr->value->resolvedType)) {
    Error::diagnostic(expr->span, "unmatched assign type");
  }

  if (expr->target->resolvedType->kind == TypeSymbol::TypeKind::CLASS) {
    Error::diagnostic(expr->span, "oberver reallocate is not allowed");
  }

  expr->resolvedType =
      implicitCasting(expr->target.get(), expr->value->resolvedType).first;
}

void Resolver::visit(MemberExpr *expr) {
  expr->object->accept(this);
  auto symbol = static_cast<TypeSymbol *>(expr->object->resolvedType);
  if (!symbol) {
    Error::internal(expr->span, "fail to cast symbol");
  }
  if (symbol->kind == TypeSymbol::TypeKind::ENUM) {
    expr->resolved = lookupEnumVariant(symbol, expr->member, expr->span);
    expr->resolvedType = symbol;
    return;
  }

  auto s = expr->object->resolvedType;

  if (dynamic_cast<GenericSymbol *>(s)) {
    Error::diagnostic(expr->span, "cannot access field with handle");
  }

  Scope *scope = nullptr;
  if (s == table->main) {
    scope = table->rootScope.get();
  } else {
    scope = s->memberScope;
  }

  if (!scope)
    Error::diagnostic(expr->span, "type has no members ");

  auto it = scope->value.find(expr->member);
  if (it == scope->value.end())
    Error::diagnostic(expr->span, "undeclared member '" + expr->member + "'");

  auto member = it->second.get();
  if (!member)
    Error::diagnostic(expr->span, "member '" + expr->member + "' is null");

  switch (member->modifier) {

  case AModifier::PUBLIC: {
    break;
  }
  case AModifier::PROTECTED: {
    if (dynamic_cast<SelfExpr *>(expr->object.get())) {
      break;
    }
    if (dynamic_cast<ThisExpr *>(expr->object.get())) {
      break;
    }
    if (dynamic_cast<SuperExpr *>(expr->object.get())) {
      break;
    }
    Error::diagnostic(expr->span,
                      "cannot access protected field in this context");
  }
  case AModifier::PRIVATE: {
    if (dynamic_cast<SelfExpr *>(expr->object.get())) {
      break;
    }
    if (dynamic_cast<ThisExpr *>(expr->object.get())) {
      break;
    }
    Error::diagnostic(expr->span,
                      "cannot access private field in this context");
  } break;
  }

  expr->resolved = member;
  expr->resolvedType = member->typeSymbol;
}

void Resolver::visit(ArrayAccessExpr *expr) {
  expr->object->accept(this);
  expr->index->accept(this);
  if (!table->isInt(expr->index->resolvedType))
    Error::diagnostic(expr->span, "array index must be integer type");
  if (auto arr = dynamic_cast<ArrayTypeSymbol *>(expr->object->resolvedType)) {
    expr->resolvedType = arr->baseType;
  } else {
    Error::diagnostic(expr->span, "not arrayType");
  }
}

void Resolver::visit(TernaryExpr *expr) {
  expr->conditon->accept(this);
  expr->then->accept(this);
  expr->else_->accept(this);
  if (!isCastable(expr->then->resolvedType, expr->else_->resolvedType)) {
    Error::diagnostic(expr->span, "unmatch then to else type");
  }
  // TODO: 삼항 연산의 최종 타입의 결정용 로직 추가 요망
  expr->resolvedType = expr->then->resolvedType;
}
void Resolver::visit(ThisExpr *expr) {
  expr->resolved = currentType;
  expr->resolvedType = currentType;
}
void Resolver::visit(SuperExpr *expr) {
  if (currentType->base == nullptr) {
    Error::diagnostic(expr->span, "this class has no baseClass");
  }
  expr->resolved = currentType->base;
  expr->resolvedType = currentType->base;
}

void Resolver::visit(RootExpr *expr) {
  expr->resolved = table->main;
  expr->resolvedType = table->main;
}
void Resolver::visit(SelfExpr *expr) {
  expr->resolved = currentType;
  expr->resolvedType = currentType;
}

void Resolver::visit(CastExpr *expr) {
  expr->left->accept(this);
  expr->type->accept(this);
  if (expr->type->resolved == nullptr) {
    Error::internal(expr->span, "type node is nullptr");
  }
  expr->resolvedType = expr->type->resolved;
}

void Resolver::visit(BuiltInNameExpr *expr) {
  expr->resolvedType = table->getBuiltName();
  switch (expr->token.kind) {
  case TKind::WORLD:
    expr->storageType = BuiltInNameExpr::StorageType::WORLD;
    break;
  case TKind::ARENA:
    expr->storageType = BuiltInNameExpr::StorageType::ARENA;
    break;
  default:
    Error::internal(expr->token, "unmatched token : " + expr->token.text);
  }
}

void Resolver::visit(SpawnExpr *expr) {
  expr->left->accept(this);
  if (expr->left->kind != NKind::BUILTIN_NAME_EXPR) {
    Error::diagnostic(expr->left->span, "expect world or arena");
  }

  if (auto b = dynamic_cast<BuiltInNameExpr *>(expr->left.get())) {
    if (b->storageType != BuiltInNameExpr::StorageType::WORLD &&
        b->storageType != BuiltInNameExpr::StorageType::ARENA) {
      Error::diagnostic(expr->left->span, "expect world or arena");
    }
  }
  expr->spawnType->accept(this);

  if (!expr->spawnType->resolved) {
    Error::internal(expr->span, "fail to resolve spawn type");
  }
  if (!expr->spawnType->resolved->memberScope) {
    Error::internal(expr->span,
                    expr->spawnType->type + "'s memberScope is nullptr");
  }
  vector<TypeSymbol *> args;
  for (auto &a : expr->args) {
    a->accept(this);
    if (!a->resolvedType) {
      Error::internal(a->span, "fail to resolve type");
    }
    args.push_back(a->resolvedType);
  }
  auto [result, method] =
      lookupMethod("init", expr->spawnType->resolved->memberScope, args);

  if (!args.empty()) {
    if (!result) {
      Error::diagnostic(expr->span,
                        "type '" + expr->spawnType->resolved->name +
                            "' does not have init but arguments were provided");
    }
  }

  if (!result) {
    Error::diagnostic(expr->span, "unmatched init argument number");
  }

  vector<TypeSymbol *> temp;
  temp.push_back(expr->spawnType->resolved);
  expr->resolvedType = table->GenericInsGetOrCreate(table->getHandle(), temp);
  expr->resolvedInit = method;
}

void Resolver::visit(ViewExpr *expr) {
  expr->left->accept(this);

  if (!expr->left->resolvedType) {
    Error::internal(expr->left->span, "fail to resolve type");
  }
  if (expr->left->resolvedType != table->getBuiltName()) {
    Error::internal(expr->span, "unmatched type");
  }

  expr->target->accept(this);
  auto generic = dynamic_cast<GenericSymbol *>(expr->target->resolvedType);
  if (!generic) {
    Error::diagnostic(expr->span, " - in view only allowed handle ");
  }
  if (generic->origin != table->getHandle()) {
    Error::diagnostic(expr->span, "in view only allowed handle");
  }

  if (!canPlaceView(
          expr->target)) { // spawnExpr등 올수 없는 형태의 표현식인지 확인
    Error::diagnostic(expr->span, "this expression not allowed here");
  }

  if (auto h = dynamic_cast<GenericSymbol *>(expr->target->resolvedType)) {
    if (h->origin != table->getHandle()) {
      Error::diagnostic(expr->span, "in view only allowed handle");
    }
    expr->resolvedType = h->args[0];
  } else {
    Error::diagnostic(expr->span, "in view only allowed handle");
  }
}

void Resolver::visit(DestroyExpr *expr) {
  expr->storage->accept(this);
  if (!expr->storage->resolvedType) {
    Error::internal(expr->storage->span, "fail to resolve type");
  }
  if (expr->storage->resolvedType != table->getBuiltName()) {
    Error::internal(expr->span, "unmatched type");
  }

  expr->target->accept(this);
  auto generic = dynamic_cast<GenericSymbol *>(expr->target->resolvedType);
  if (!generic) {
    Error::diagnostic(expr->span, " in view only allowed handle");
  }
  if (generic->origin != table->getHandle()) {
    Error::diagnostic(expr->span, "in view only allowed handle");
  }

  if (!canPlaceView(
          expr->target)) { // spawnExpr등 올수 없는 형태의 표현식인지 확인
    Error::diagnostic(expr->span, "this expression not allowed here");
  }

  if (auto h = dynamic_cast<GenericSymbol *>(expr->target->resolvedType)) {
    if (h->origin != table->getHandle()) {
      Error::diagnostic(expr->span, "in view only allowed handle");
    }
    expr->resolvedType = h->args[0];
  } else {
    Error::diagnostic(expr->span, "in view only allowed handle");
  }
}
void Resolver::visit(QuitExpr *expr) {
  for (Scope *s = table->getCurrent(); s != nullptr; s = s->parent) {
    if (s->scopeKind == Scope::ScopeKind::INIT) {
      Error::diagnostic(expr->span, "in init method cannot use quit");
    }
  }
}

void Resolver::visit(DefaultValueExpr *expr) {
  expr->resolvedType = table->getDefaultV();
  if (!expr->resolvedType) {
    Error::internal(expr->span, "resolved Type is nullptr");
  }
}

void Resolver::visit(Range *expr) {
  expr->from->accept(this);
  if (!table->isInt(expr->from->resolvedType)) {
    Error::diagnostic(expr->span, "non-int range not supported yet");
  }

  expr->to->accept(this);
  if (!table->isInt(expr->to->resolvedType)) {
    Error::diagnostic(expr->span, "non-int range not supported yet");
  }

  if (!expr->step) {
    Token step;
    step.span = expr->span;
    step.kind = TKind::LIT_INT;
    step.text = "1";
    expr->step = make_shared<LiteralExpr>(expr->span, step, "1");
  }
  expr->step->accept(this);
  if (!table->isInt(expr->step->resolvedType)) {
    Error::diagnostic(expr->span, "in for-range step only allowed int type");
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
        Error::internal(lit->span, "duplicated case key");
      }
    }
    if (auto m = dynamic_cast<MatchExpr *>(currentSwitch)) {
      if (!m->caseKeys.insert(key).second) {
        Error::internal(lit->span, "duplicated case key");
      }
    }

    return;
  }

  if (dynamic_cast<DefaultValueExpr *>(expr->value.get())) {
    expr->isWildCard = true;
    return;
  }

  string name = "";
  TypeSymbol *enumTarget = getTargetType();
  if (auto n = dynamic_cast<NameExpr *>(expr->value.get())) {
    name = n->name;
  } else if (auto m = dynamic_cast<MemberExpr *>(expr->value.get())) {
    name = m->member;
    m->object->accept(this);
    if (m->object->resolvedType != enumTarget) {
      Error::diagnostic(m->span, "unmatched enum type");
    }
  } else if (auto c = dynamic_cast<CallExpr *>(expr->value.get())) {
    name = c->methodName;
    if (c->receiver != nullptr) {
      c->receiver->accept(this);
      if (c->receiver->resolvedType != enumTarget) {
        Error::diagnostic(c->span, "unmatched enum type");
      }
    }

  } else {
    Error::diagnostic(
        expr->span, "not allowed non-literal value or not enum-variant value");
  }

  if (enumTarget->kind != TypeSymbol::TypeKind::ENUM) {
    Error::diagnostic(
        expr->span, "not allowed non-literal value or not enum-variant value");
  }

  auto it = enumTarget->variantMap.find(name);
  if (it == enumTarget->variantMap.end()) {
    Error::diagnostic(expr->value->span, "unknown variant name");
  }

  if (expr->arg) {
    if (it->second->payloadType == nullptr) {
      Error::diagnostic(expr->arg->span,
                        "variant has no payload but in use has payload");
    }
    if (auto n = dynamic_cast<NameExpr *>(expr->arg.get())) {

      unique_ptr<ValueSymbol> symbol = make_unique<ValueSymbol>();
      symbol->typeSymbol = it->second->payloadType;
      symbol->isPayload = true;
      symbol->isRoot = false;
      symbol->kind = ValueSymbol::Kind::VAR;
      symbol->owner = table->getCurrent();
      symbol->name = n->name;
      expr->payloadType = it->second->payloadType;
      auto raw = symbol.get();
      expr->payload = raw;
      auto iter = table->getCurrent()->value.find(n->name);

      if (iter == table->getCurrent()->value.end()) {
        table->getCurrent()->value.emplace(n->name, std::move(symbol));
      } else {
        Error::diagnostic(iter->second->nameSpan, "duplicated variable name");
      }

    } else {
      Error::diagnostic(expr->arg->span,
                        "in variant payload must be payload's name");
    }
  }

  CaseKey key = CaseKey(it->second);

  if (auto s = dynamic_cast<SwitchStmt *>(currentSwitch)) {
    if (!s->caseKeys.insert(key).second) {
      Error::internal(expr->value->span, "duplicated case key");
    }
    s->usedVariants.insert(it->second);
  }
  if (auto m = dynamic_cast<MatchExpr *>(currentSwitch)) {
    if (!m->caseKeys.insert(key).second) {
      Error::internal(expr->value->span, "duplicated case key");
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

  if (auto s = dynamic_cast<MatchExpr *>(currentSwitch)) {
    if (s->value->resolvedType->kind == TypeSymbol::TypeKind::ENUM) {
      return s->value->resolvedType;
    }
  }
  return nullptr;
}

void Resolver::visit(MatchExpr *expr) {
  ScopeGuard _(*table, expr->blockScope);
  auto prev = currentSwitch;
  currentSwitch = expr;
  expr->value->accept(this);
  TypeSymbol *matchType = nullptr;
  for (unsigned i = 0; i < expr->cases.size(); ++i) {
    auto &c = expr->cases[i];
    c->accept(this);

    if (!matchType) {
      matchType = c->transferType;
      continue;
    }
    if (!isAssignable(matchType, c->transferType)) {
      Error::diagnostic(c->span, "inconsistent value transfer type");
    }

    if (c->isWildCard) {
      if (i != expr->cases.size() - 1) {
        Error::diagnostic(c->span, "_ is only allowed in last of match");
      }
      expr->hasDefault = true;
    }
  }

  if (expr->usedVariants.size() != expr->value->resolvedType->variants.size()) {
    if (!expr->hasDefault) {
      Error::diagnostic(expr->span, "has missing variant but no _ in switch");
    }
  }

  if (expr->value->resolvedType->kind == TypeSymbol::TypeKind::PRIMITIVE) {
    if (!expr->hasDefault) {
      Error::diagnostic(expr->span, "literal target but no _ in switch");
    }
  }

  expr->resolvedType = matchType;
  currentSwitch = prev;
}