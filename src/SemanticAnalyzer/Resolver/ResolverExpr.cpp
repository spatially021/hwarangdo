#include "AST/ASTNode.h"
#include "AST/Decl.h"
#include "AST/Expr.h"
#include "AST/Stmt.h"
#include "SemanticAnalyzer/ResolvedLit.h"
#include "SemanticAnalyzer/Resolver.h"
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
    if (expr->tOp.kind == TKind::PLUS) {
      expr->op = Operator::PLUS;
    } else {
      expr->op = Operator::MINUS;
    }
    if (table->isNumberic(expr->right->resolvedType))
      expr->resolvedType = expr->right->resolvedType;
    else
      Error::diagnostic(expr->span,
                        "bad operand type " + expr->right->resolvedType->name +
                            " for unary operator '" + expr->tOp.text + "'");
  }
}

void Resolver::visit(AssignExpr *expr) {
  expr->target->accept(this);
  expr->value->accept(this);
  if (!isAssignable(expr->target->resolvedType, expr->value->resolvedType)) {
    Error::diagnostic(expr->span, "unmatched assign type");
  }

  expr->resolvedType =
      implicitCasting(expr->target->resolvedType, expr->value->resolvedType);
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

  expr->resolved = member;
  expr->resolvedType = member->typeSymbol;
}

void Resolver::visit(ArrayAccessExpr *expr) {
  expr->object->accept(this);
  expr->index->accept(this);
  if (!table->isInt(expr->index->resolvedType))
    Error::diagnostic(expr->span, "array index must be integer type");
  auto arr = static_cast<TypeSymbol *>(expr->object->resolvedType);
  if (arr->decl->kind != NKind::ARRAY_DECL)
    Error::diagnostic(expr->span, "type is not indexable");
  expr->resolvedType = static_cast<ArrayDecl *>(arr->decl)->baseType;
}

void Resolver::visit(TernaryExpr *expr) {
  expr->conditon->accept(this);
  expr->then->accept(this);
  expr->else_->accept(this);
  if (!isCastable(expr->then->resolvedType, expr->else_->resolvedType)) {
    Error::diagnostic(expr->span, "unmatch then to else type");
  }
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

void Resolver::visit(DefaultValueExpr *expr) {
  expr->resolvedType = table->getDefaultV();
  if (!expr->resolvedType) {
    Error::internal(expr->span, "resolved Type is nullptr");
  }
}

void Resolver::visit(Range *expr) {
  expr->from->accept(this);
  if (!table->isInt(expr->from->resolvedType)) {
    Error::diagnostic(expr->span, "in for-range start only allowed int type");
  }

  expr->to->accept(this);
  if (!table->isInt(expr->to->resolvedType)) {
    Error::diagnostic(expr->span, "in for-range end only allowed int type");
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
}

void Resolver::visit(CaseValueExpr *expr) {

  auto type = getTargetType();
  if (type) {
    string variantName = "";
    if (expr->value->kind == NKind::MEMBER_EXPR) {
      auto temp = dynamic_cast<MemberExpr *>(expr->value.get());
      if (temp->object->resolvedType != type) {
        Error::diagnostic(expr->span, "unmatch enum type expect " + type->name +
                                          " but " +
                                          temp->object->resolvedType->name);
      }
      variantName = temp->member;
    } else if (auto temp = dynamic_cast<NameExpr *>(expr->value.get())) {
      variantName = temp->name;
    } else {
      Error::internal(expr->span, "illegal expr kind");
    }

    expr->variant = lookupEnumVariant(type, variantName, expr->span);
  } else {
    expr->value->accept(this);
    if (isLit(expr->value)) {
      if (expr->arg) {
        Error::internal(expr->span, "case value is lit but has payload");
      }
      if (auto s = dynamic_cast<SwitchStmt *>(currentSwitch)) {
        if (!canImplicitlyConvert(expr->value->resolvedType,
                                  s->value->resolvedType)) {
          Error::diagnostic(expr->span, "unmatched case valueType");
        }
      } else if (auto m = dynamic_cast<MatchExpr *>(currentSwitch)) {
        if (!canImplicitlyConvert(expr->value->resolvedType,
                                  m->value->resolvedType)) {
          Error::diagnostic(expr->span, "unmatched case valueType");
        }
      } else {
        Error::internal(expr->span, "currentSwitch is not swtich or match");
      }

    } else {
      Error::diagnostic(expr->span, "in caseValue allow literal or Enum");
    }
    return;
  }

  auto variant = dynamic_cast<EnumVariantSymbol *>(expr->variant);
  if (!variant) {
    Error::diagnostic(expr->span, "in caseValue allow literal or Enum");
  }
  if (variant->payloadType) {
    if (!expr->arg) {
      Error::diagnostic(expr->span, "this variant has payload but not declare");
    }

    if (expr->arg->kind != NKind::NAME_EXPR) {
      Error::internal(expr->span, "illegal expr kind");
    }
    expr->arg->resolvedType = variant->payloadType;
    if (!expr->arg->resolvedType) {
      Error::internal(expr->arg->span, "unresolved type ");
    }
    if (!canImplicitlyConvert(expr->arg->resolvedType, variant->payloadType)) {
      Error::diagnostic(expr->span, "unmatched payload type");
    }
    auto symbol = make_unique<ValueSymbol>();
    symbol->name = variant->name;
    symbol->kind = ValueSymbol::Kind::VAR;
    symbol->typeSymbol = expr->arg->resolvedType;
    auto raw = symbol.get();

    static_cast<NameExpr *>(expr->arg.get())->resolved = raw;

    expr->payloadType = expr->arg->resolvedType;
    return;
  }
  if (expr->arg) {
    Error::diagnostic(expr->span, "variant has no payload but declared");
  }
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
  for (auto c : expr->cases) {
    c->accept(this);
    if (!matchType) {
      matchType = c->transferType;
      continue;
    }
    if (!isAssignable(matchType, c->transferType)) {
      Error::diagnostic(c->span, "inconsistent value transfer type");
    }
  }

  expr->resolvedType = matchType;
  currentSwitch = prev;
}