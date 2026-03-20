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
#include "util/Error.h"
#include "util/Guard.h"
#include <cstddef>
#include <memory>
#include <vector>

void Resolver::visit(LiteralExpr *expr) {
  ResolvedLit r;
  switch (expr->token.kind) {
  case TKind::LIT_INT:
    r = resolveLitInt(expr);
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
    expr->resolvedType = table->getType("bool");
    r.type = table->getType("bool");
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
    Error::internal(expr->left->token,
                    "lhs is nullptr : " + expr->left->token.text);
  }

  if (expr->right->resolvedType == nullptr) {
    Error::internal(expr->right->token,
                    "rhs is nullptr : " + expr->right->token.text);
  }

  if (isBinaryOperatalbe(expr->op, expr->left->resolvedType,
                         expr->right->resolvedType)) {
    auto temp = binaryResult(expr->op, expr->left->resolvedType,
                             expr->right->resolvedType);
    if (!temp) {
      Error::internal(expr->token, "fail to get binaryResult");
    }
    expr->resolvedType = temp;
  } else {
    Error::diagnostic(expr->token, "leftExpr and rightExpr cannot operate. [ " +
                                       expr->left->resolvedType->name + " " +
                                       expr->opRaw.text + " " +
                                       expr->right->resolvedType->name + " ]");
  }

  if (!expr->resolvedType) {
    Error::internal(expr->token, "unresolved type : " + expr->token.text);
  }
}
void Resolver::visit(NameExpr *expr) {
  auto *symbol = resolveValue(expr->name);

  if (!symbol) {
    auto *temp = table->getType(expr->name);
    if (temp) {
      if (temp->kind == TypeSymbol::TypeKind::ENUM) {
        expr->resolvedType = temp;
        expr->typeSymbol = temp;
        return;
      }
      Error::internal(expr->token, "expect enum");
    }

    if (currentType->baseName.has_value() && currentType->base) {
      symbol = lookLocalValue(expr->name, currentType->base->memberScope);
    }

    if (!symbol) {
      Error::diagnostic(expr->token, "undeclared variable: " + expr->name);
    }
  }

  expr->valueSymbol = symbol;

  if (!expr->valueSymbol) {
    Error::internal(expr->token, "resolved symbol is nullptr");
  }

  if (!expr->valueSymbol->typeSymbol) {
    Error::internal(expr->token, "typeSymbol is nullptr");
  }

  expr->resolvedType = expr->valueSymbol->typeSymbol;

  if (!expr->resolvedType) {
    Error::internal(expr->token, "type symbol is nullptr: " + expr->token.text);
  }

  if (dynamic_cast<HandleSymbol *>(expr->resolvedType)) {
    Error::internal(expr->token, "handle gotten");
  }
}

void Resolver::visit(UnaryExpr *expr) {
  expr->right->accept(this);

  if (expr->op.kind == TKind::BANG) {
    if (expr->right->resolvedType == table->getType("bool"))
      expr->resolvedType = expr->right->resolvedType;
    else if (expr->right->resolvedType == table->getType("int")) {
      expr->resolvedType = expr->right->resolvedType;
    } else
      Error::diagnostic(expr->token, "bad operand type " +
                                         expr->right->resolvedType->name +
                                         " for unary operator '!'");
  } else if (expr->op.kind == TKind::PLUS || expr->op.kind == TKind::MINUS) {

    if (table->isNumberic(expr->right->resolvedType))
      expr->resolvedType = expr->right->resolvedType;
    else
      Error::diagnostic(expr->token,
                        "bad operand type " + expr->right->resolvedType->name +
                            " for unary operator '" + expr->op.text + "'");
  }
}

void Resolver::visit(AssignExpr *expr) {
  expr->target->accept(this);
  expr->value->accept(this);
  if (!isAssignable(expr->target->resolvedType, expr->value->resolvedType)) {
    Error::diagnostic(expr->token, "unmatched assign type");
  }
  expr->resolvedType =
      implicitCasting(expr->target->resolvedType, expr->value->resolvedType);
}

void Resolver::visit(MemberExpr *expr) {
  expr->object->accept(this);
  auto symbol = static_cast<TypeSymbol *>(expr->object->resolvedType);
  if (!symbol) {
    Error::internal(expr->token,
                    "fail to cast symbol : " + expr->object->token.text);
  }
  if (symbol->kind == TypeSymbol::TypeKind::ENUM) {
    expr->resolved = lookupEnumVariant(symbol, expr->member, expr->token);
    expr->resolvedType = symbol;
    return;
  }

  auto s = expr->object->resolvedType;
  if (dynamic_cast<GenericSymbol *>(s)) {
    Error::diagnostic(expr->token, "cannot access field with handle : " +
                                       expr->object->token.text);
  }
  auto scope = s->memberScope;
  if (!scope)
    Error::diagnostic(expr->token, "type has no members : " + expr->token.text);

  auto it = scope->value.find(expr->member);
  if (it == scope->value.end())
    Error::diagnostic(expr->token, "undeclared member '" + expr->member + "'");

  auto member = it->second.get();
  if (!member)
    Error::diagnostic(expr->token, "member '" + expr->member + "' is null");

  expr->resolved = member;
  expr->resolvedType = member->typeSymbol;
}

void Resolver::visit(ArrayAccessExpr *expr) {
  expr->object->accept(this);
  expr->index->accept(this);
  if (!table->isInt(expr->index->resolvedType))
    Error::diagnostic(expr->token, "array index must be integer type");
  auto arr = static_cast<TypeSymbol *>(expr->object->resolvedType);
  if (arr->decl->kind != NKind::ARRAY_DECL)
    Error::diagnostic(expr->token, "type is not indexable");
  expr->resolvedType = static_cast<ArrayDecl *>(arr->decl)->baseType;
}

void Resolver::visit(TernaryExpr *expr) {
  expr->conditon->accept(this);
  expr->then->accept(this);
  expr->else_->accept(this);
  if (!isCastable(expr->then->resolvedType, expr->else_->resolvedType)) {
    Error::diagnostic(expr->token, "unmatch then to else type");
  }
}
void Resolver::visit(ThisExpr *expr) {
  expr->resolved = currentType;
  expr->resolvedType = currentType;
}
void Resolver::visit(SuperExpr *expr) {
  if (currentType->base == nullptr) {
    Error::diagnostic(expr->token, "this class has no baseClass");
  }
  expr->resolved = currentType->base;
  expr->resolvedType = currentType->base;
}

void Resolver::visit(CastExpr *expr) {
  expr->left->accept(this);
  expr->type->accept(this);
  if (expr->type->resolved == nullptr) {
    Error::internal(expr->token, "type node is nullptr : " + expr->token.text);
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
    Error::diagnostic(expr->token,
                      "expect world or arena but " + expr->left->token.text);
  }

  if (auto b = dynamic_cast<BuiltInNameExpr *>(expr->left.get())) {
    if (b->storageType != BuiltInNameExpr::StorageType::WORLD &&
        b->storageType != BuiltInNameExpr::StorageType::ARENA) {
      Error::diagnostic(expr->token,
                        "expect world or arena but " + expr->left->token.text);
    }
  }

  if (!expr->spawnType->resolved) {
    Error::internal(expr->token, "fail to resolve spawn type");
  }
  if (!expr->spawnType->resolved->memberScope) {
    Error::internal(expr->token,
                    expr->token.text + "'s memberScope is nullptr");
  }
  vector<TypeSymbol *> args;
  for (auto &a : expr->args) {
    a->accept(this);
    if (!a->resolvedType) {
      Error::internal(a->token, "fail to resolve type : " + a->token.text);
    }
    args.push_back(a->resolvedType);
  }
  auto it = expr->spawnType->resolved->memberScope->method.find("init");
  if (it == expr->spawnType->resolved->memberScope->method.end()) {
    if (args.size() != 0) {
      Error::diagnostic(expr->token,
                        "type '" + expr->spawnType->resolved->name +
                            "' does not have init but arguments were provided");
    }
  } else {
    auto init = it->second.get();
    if (init->paramTypes.size() != args.size()) {
      Error::diagnostic(expr->token, "unmatched init argument number");
    }
    for (size_t i = 0; i < init->paramTypes.size(); ++i) {
      if (!isAssignable(args[i], init->paramTypes[i])) {
        Error::diagnostic(expr->args[i]->token,
                          "unmatched argument type expect '" +
                              init->paramTypes[i]->name + "' but " +
                              args[i]->name);
      }
    }
  }

  vector<TypeSymbol *> temp;
  temp.push_back(expr->spawnType->resolved);
  expr->resolvedType = table->GenericInsGetOrCreate(table->getHandle(), temp);
}

void Resolver::visit(ViewExpr *expr) {
  expr->left->accept(this);

  if (!expr->left->resolvedType) {
    Error::internal(expr->left->token,
                    "fail to resolve type : " + expr->left->token.text);
  }
  if (expr->left->resolvedType != table->getBuiltName()) {
    Error::internal(expr->token, "unmatched type : " + expr->left->token.text);
  }

  expr->target->accept(this);
  auto generic = dynamic_cast<GenericSymbol *>(expr->target->resolvedType);
  if (!generic) {
    Error::diagnostic(expr->token, expr->target->token.text +
                                       " - in view only allowed handle : " +
                                       expr->target->resolvedType->name);
  }
  if (generic->origin != table->getHandle()) {
    Error::diagnostic(expr->token, "in view only allowed handle : " +
                                       expr->target->token.text);
  }

  if (!canPlaceView(
          expr->target)) { // spawnExpr등 올수 없는 형태의 표현식인지 확인
    Error::diagnostic(expr->token, "this expression not allowed here");
  }

  if (auto h = dynamic_cast<GenericSymbol *>(expr->target->resolvedType)) {
    if (h->origin != table->getHandle()) {
      Error::diagnostic(expr->token, "in view only allowed handle");
    }
    expr->resolvedType = h->args[0];
  } else {
    Error::diagnostic(expr->token, "in view only allowed handle");
  }
}

void Resolver::visit(DefaultValueExpr *expr) {
  expr->resolvedType = table->getDefaultV();
  if (!expr->resolvedType) {
    Error::internal(expr->token,
                    "resolved Type is nullptr : " + expr->token.text);
  }
}

void Resolver::visit(Range *expr) {
  expr->from->accept(this);
  expr->to->accept(this);
  if (expr->step) {
    expr->step->accept(this);
  }
}

void Resolver::visit(CaseValueExpr *expr) {

  auto type = getTargetType();
  if (type) {
    string variantName = "";
    if (expr->value->kind == NKind::MEMBER_EXPR) {
      auto temp = dynamic_cast<MemberExpr *>(expr->value.get());
      if (temp->object->resolvedType != type) {
        Error::diagnostic(expr->token, "unmatch enum type expect " +
                                           type->name + " but " +
                                           temp->object->resolvedType->name);
      }
      variantName = temp->member;
    } else if (expr->value->kind == NKind::NAME_EXPR) {
      variantName = expr->value->token.text;
    } else {
      Error::internal(expr->token, "illegal expr kind");
    }

    expr->variant = lookupEnumVariant(type, variantName, expr->token);
  } else {
    expr->value->accept(this);
    if (isLit(expr->value)) {
      if (expr->arg) {
        Error::internal(expr->token, "case value is lit but has payload");
      }
      if (auto s = dynamic_cast<SwitchStmt *>(currentSwitch)) {
        if (!canImplicitlyConvert(expr->value->resolvedType,
                                  s->value->resolvedType)) {
          Error::diagnostic(expr->token, "unmatched case valueType");
        }
      } else if (auto m = dynamic_cast<MatchExpr *>(currentSwitch)) {
        if (!canImplicitlyConvert(expr->value->resolvedType,
                                  m->value->resolvedType)) {
          Error::diagnostic(expr->token, "unmatched case valueType");
        }
      } else {
        Error::internal(expr->token, "currentSwitch is not swtich or match");
      }

    } else {
      Error::diagnostic(expr->token, "in caseValue allow literal or Enum");
    }
    return;
  }

  auto variant = dynamic_cast<EnumVariantSymbol *>(expr->variant);
  if (!variant) {
    Error::diagnostic(expr->token, "in caseValue allow literal or Enum");
  }
  if (variant->payloadType) {
    if (!expr->arg) {
      Error::diagnostic(expr->token,
                        "this variant has payload but not declare");
    }

    if (expr->arg->kind != NKind::NAME_EXPR) {
      Error::internal(expr->token, "illegal expr kind");
    }
    expr->arg->resolvedType = variant->payloadType;
    if (!expr->arg->resolvedType) {
      Error::internal(expr->arg->token,
                      "unresolved type : " + expr->arg->token.text);
    }
    if (!canImplicitlyConvert(expr->arg->resolvedType, variant->payloadType)) {
      Error::diagnostic(expr->token, "unmatched payload type");
    }
    auto symbol = make_unique<ValueSymbol>();
    symbol->name = expr->arg->token.text;
    symbol->kind = ValueSymbol::Kind::VAR;
    symbol->typeSymbol = expr->arg->resolvedType;
    auto raw = symbol.get();

    static_cast<NameExpr *>(expr->arg.get())->valueSymbol = raw;

    expr->payloadType = expr->arg->resolvedType;
    return;
  }
  if (expr->arg) {
    Error::diagnostic(expr->token, "variant has no payload but declared");
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
      Error::diagnostic(c->token, "inconsistent value transfer type");
    }
  }

  expr->resolvedType = matchType;
  currentSwitch = prev;
}