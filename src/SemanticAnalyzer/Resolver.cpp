#include "SemanticAnalyzer/Resolver.h"
#include "AST/ASTNode.h"
#include "AST/Expr.h"
#include "SemanticAnalyzer/Guard.h"
#include "SemanticAnalyzer/Scope.h"
#include "SemanticAnalyzer/Symbol.h"
#include "SemanticAnalyzer/SymbolTable.h"
#include "util/Error.h"
#include <memory>
#include <string>
#include <utility>

Resolver::Resolver(SymbolTable *t) : table(t) {}

void Resolver::visit(LiteralExpr *expr) {
  switch (expr->token.kind) {
  case TKind::LIT_INT:
    expr->resolvedType = table->getType("int");
    break;
  case TKind::LIT_FLOAT:
    expr->resolvedType = table->getType("float");
    break;
  case TKind::FIXED:
    expr->resolvedType = table->getType("fixed");
    break;
  case TKind::LIT_CHARACTER:
    expr->resolvedType = table->getType("char");
    break;
  case TKind::LIT_STRING:
    expr->resolvedType = table->getType("string");
    break;
  case TKind::LIT_BOOL:
    expr->resolvedType = table->getType("bool");
    break;
  default:
    Error::diagnostic(expr->token, "unknown literal type");
  }
}
void Resolver::visit(BinaryExpr *expr) {
  expr->left->accept(this);
  expr->right->accept(this);

  if (expr->left->state == Expr::State::UNKNOWN ||
      expr->right->state == Expr::State::UNKNOWN ||
      expr->left->state == Expr::State::NEED_CHECK ||
      expr->right->state == Expr::State::NEED_CHECK) {
    expr->resolvedType = table->getUnknown();
    expr->state = Expr::State::UNKNOWN;
  } else if (expr->left->resolvedType == expr->right->resolvedType) {
    TKind kind = expr->op.kind;
    if (kind == TKind::GREATER || kind == TKind::GREATER_EQUAL ||
        kind == TKind::LESS || kind == TKind::LESS_EQUAL) {
      expr->resolvedType = table->getType("bool");
    } else
      expr->resolvedType = expr->left->resolvedType;
  } else {
    expr->resolvedType = table->getUnknown();
    expr->state = Expr::State::NEED_CHECK;
  }
}
void Resolver::visit(VarExpr *expr) {
  auto symbol = resolveValue(expr->name);
  if (!symbol) {
    auto temp = table->getType(expr->name);
    if (temp) {
      if (temp->kind == TypeSymbol::Kind::ENUM) {
        expr->resolvedType = temp;
        return;
      }
    }

    if (currentType->baseName.has_value()) {
      if (currentType->base) {
        symbol = lookLocalValue(expr->name, currentType->base->memberScope);
      }
    }

    if (!symbol)
      Error::diagnostic(expr->token, "undeclare variable: " + expr->name);
  }

  expr->resolvedType = symbol->typeSymbol;
  expr->resolved = symbol;
}
void Resolver::visit(UnaryExpr *expr) {
  expr->right->accept(this);
  if (expr->right->resolvedType == table->getUnknown()) {
    expr->resolvedType = table->getUnknown();
    return;
  }

  if (expr->op.kind == TKind::BANG) {
    if (expr->right->resolvedType == table->getType("bool"))
      expr->resolvedType = expr->right->resolvedType;
    else
      Error::diagnostic(expr->token, "bad operand type " +
                                         expr->right->resolvedType->name +
                                         " for unary operator '!'");
  } else if (expr->op.kind == TKind::PLUS || expr->op.kind == TKind::MINUS) {
    auto type = expr->right->resolvedType;
    if (table->isNumberic(type))
      expr->resolvedType = type;
    else
      Error::diagnostic(expr->token,
                        "bad operand type " + expr->right->resolvedType->name +
                            " for unary operator '" + expr->op.text + "'");
  }
}

void Resolver::ResolveEnumVariant(CallExpr *expr) {
  if (expr->arguments.size() > 1)
    Error::diagnostic(expr->token, "in variant only one payload allowed");
  if (expr->arguments.size() == 1) {
    expr->arguments[0]->accept(this);
    expr->VariantResolved->payloadType = expr->arguments[0]->resolvedType;
  }

  auto variant = expr->VariantResolved;

  if (expr->arguments.size() == 1) {
    if (variant->payloadType == nullptr)
      Error::diagnostic(expr->token, "this variant need payload");
    else if (variant->payloadType != expr->arguments[0]->resolvedType)
      Error::diagnostic(expr->token, "incorrect payload type");
  } else {
    if (variant->payloadType != nullptr)
      Error::diagnostic(expr->token, expr->methodName + " needs payload");
  }

  expr->resolvedType = expr->receiver->resolvedType;
}
void Resolver::ResolveCall(CallExpr *expr) {
  for (auto a : expr->arguments)
    a->accept(this);

  auto method = expr->methodResolved;

  auto funcDecl = dynamic_cast<FuncDecl *>(method->decl);
  if (!funcDecl) {
    Error::internal("method declaration is not FuncDecl");
    return;
  }

  if (expr->arguments.size() != funcDecl->params.size())
    Error::diagnostic(expr->token, "mismatch argument count");

  expr->resolvedType = method->returnType;
}

void Resolver::visit(CallExpr *expr) {
  if (expr->callType == CallExpr::CallType::UNRESOLVED) {
    if (expr->receiver != nullptr) {
      expr->receiver->accept(this);
      if (expr->receiver->resolvedType->kind == TypeSymbol::Kind::ENUM) {
        auto symbol = expr->receiver->resolvedType;
        auto it_p = symbol->variantMap.find(expr->methodName);
        if (it_p == symbol->variantMap.end())
          Error::diagnostic(expr->token,
                            expr->methodName + "is not enum variant");
        expr->callType = CallExpr::CallType::PAYLOAD_CALL;
        expr->VariantResolved = it_p->second;
        ResolveEnumVariant(expr);
      } else {
        expr->callType = CallExpr::CallType::FUNC_CALL;

        Scope *scope = nullptr;

        if (expr->receiver != nullptr) {
          if (!expr->receiver->resolvedType) {
            Error::diagnostic(expr->token, "invalid call receiver");
          }

          scope = expr->receiver->resolvedType->memberScope;
          if (!scope) {
            Error::diagnostic(expr->token, "type has no members");
          }
        } else {
          scope = currentType->memberScope;
          if (!scope) {
            Error::internal("scope is null");
          }
        }

        auto it = scope->method.find(expr->methodName);
        if (it == scope->method.end()) {
          Error::diagnostic(expr->token,
                            "'" + expr->methodName + "' is not a method");
        }

        MethodSymbol *method = it->second.get();
        if (!method) {
          Error::diagnostic(expr->token, "'" + expr->methodName +
                                             "' is not a method member");
        }
        expr->methodResolved = method;
        ResolveCall(expr);
      }
    }
  }
}

void Resolver::visit(AssignExpr *expr) {
  expr->target->accept(this);
  expr->value->accept(this);
  if (expr->value->state == Expr::State::UNKNOWN ||
      expr->value->state == Expr::State::NEED_CHECK) {
    expr->resolvedType = table->getUnknown();
    expr->state = Expr::State::UNKNOWN;
  } else if (expr->target->resolvedType == expr->value->resolvedType)
    expr->resolvedType = expr->target->resolvedType;
  else {
    expr->resolvedType = table->getUnknown();
    expr->state = Expr::State::NEED_CHECK;
  }
}
void Resolver::visit(MemberExpr *expr) {
  expr->object->accept(this);

  if (!expr->object->resolvedType)
    Error::diagnostic(expr->token, "invalid member access on null type");

  if (expr->object->resolvedType->kind == TypeSymbol::Kind::ENUM) {
    auto it = expr->object->resolvedType->variantMap.find(expr->member);
    if (it == expr->object->resolvedType->variantMap.end()) {
      Error::diagnostic(expr->token, expr->member + " is not " +
                                         expr->object->resolvedType->name +
                                         "'s variant");
    }
    expr->variantResolved = it->second;
    expr->resolvedType = expr->object->resolvedType;
  } else {
    auto scope = expr->object->resolvedType->memberScope;
    if (!scope)
      Error::diagnostic(expr->token, "type has no members");

    auto it = scope->value.find(expr->member);
    if (it == scope->value.end())
      Error::diagnostic(expr->token,
                        "undeclared member '" + expr->member + "'");

    auto member = it->second.get();
    if (!member)
      Error::diagnostic(expr->token, "member '" + expr->member + "' is null");

    expr->valeuResolved = member;
    expr->resolvedType = member->typeSymbol;
  }
}

void Resolver::visit(ArrayAccessExpr *expr) {
  expr->object->accept(this);
  expr->index->accept(this);
  if (expr->index->resolvedType != table->getType("int"))
    Error::diagnostic(expr->token, "array index must be integer type");
  auto arr = expr->object->resolvedType;
  if (arr->decl->kind != NKind::ARRAY_DECL)
    Error::diagnostic(expr->token, "type is not indexable");
  expr->resolvedType = static_cast<ArrayDecl *>(arr->decl)->baseType;
}

void Resolver::visit(TernaryExpr *expr) {
  expr->conditon->accept(this);
  expr->then->accept(this);
  expr->else_->accept(this);
  if (expr->then->resolvedType != expr->else_->resolvedType)
    expr->resolvedType = table->getUnknown();
  else
    expr->resolvedType = expr->then->resolvedType;
}
void Resolver::visit(ThisExpr *expr) {
  expr->resolved = currentType;
  expr->resolvedType = currentType;
}
void Resolver::visit(SuperExpr *expr) {
  expr->resolved = currentType->base;
  expr->resolvedType = currentType->base;
}

// Statement Resolver::visitor methods
void Resolver::visit(ExprStmt *stmt) { stmt->expr->accept(this); }
void Resolver::visit(BlockStmt *stmt) {
  ScopeGuard _(*table, stmt->blockScope);
  for (auto a : stmt->statements)
    a->accept(this);
}
void Resolver::visit(IfStmt *stmt) {
  stmt->condition->accept(this);
  if (stmt->condition->resolvedType != table->getType("bool")) {
    if (stmt->condition->resolvedType != table->getUnknown()) {
      Error::diagnostic(stmt->condition->token, "condition is not bool type");
    }
  }

  stmt->thenBranch->accept(this);
  if (stmt->elseBranch != nullptr)
    stmt->elseBranch->accept(this);
}
void Resolver::visit(ForStmt *stmt) {
  stmt->initializer->accept(this);
  ScopeGuard _(*table, stmt->blockScope);
  stmt->body->accept(this);
}
void Resolver::visit(WhileStmt *stmt) {
  stmt->condition->accept(this);
  if (stmt->condition->resolvedType != table->getType("bool") &&
      stmt->condition->resolvedType != table->getUnknown())
    Error::diagnostic(stmt->condition->token, "condition is not bool type");
  stmt->body->accept(this);
}
void Resolver::visit(SwitchStmt *stmt) {
  for (auto c : stmt->clauses)
    c->accept(this);
}
void Resolver::visit(Case *stmt) { stmt->body->accept(this); }

void Resolver::visit(ReturnStmt *stmt) {
  if (stmt->value != nullptr) {
    stmt->value->accept(this);
    if (stmt->value->resolvedType == nullptr) {
      Error::internal("return value is nullptr");
    }
    stmt->resolved = stmt->value->resolvedType;
  } else {
    stmt->resolved = table->getType("void");
  }
  currentMethod->returns.push_back(stmt);
}
void Resolver::visit(BreakStmt *) {}
void Resolver::visit(ContinueStmt *) {}
void Resolver::visit(DeclStmt *stmt) { stmt->decl->accept(this); }
void Resolver::visit(EmptyStmt *) {}

void Resolver::visit(ClassDecl *decl) {

  auto symbol = decl->symbol;
  if (decl->baseClass.has_value()) {
    symbol->base = table->getType(decl->baseClass.value());
  }

  ScopeGuard _(*table, decl->symbol->memberScope);
  TypeContextGuard __(currentType, decl->symbol);

  for (auto a : decl->body) {
    a->accept(this);
  }
}

void Resolver::visit(StructDecl *decl) {
  if (!decl->symbol) {
    Error::diagnostic(decl->token, "StructDecl symbol not initialized");
    return;
  }
  ScopeGuard _(*table, decl->symbol->memberScope);
  for (auto a : decl->fields) {
    a->accept(this);
  }
}
void Resolver::visit(EnumDecl *) {}
void Resolver::visit(ImplDecl *decl) {
  string s = decl->target;

  auto it = table->implMap.find(decl);
  if (it == table->implMap.end()) {
    Error::internal("this impl is not declared");
  }

  ImplSymbol *symbol = it->second;

  symbol->target = table->getType(decl->target);

  ScopeGuard _(*table, symbol->memberScope);

  Scope *prevSelfScope = currentSelf;
  currentSelf = symbol->target->memberScope;
  for (auto a : decl->LinkedImplMethods) {
    a->accept(this);
    if (a->methodSymbol == nullptr) {
      Error::internal("method symbol is null");
    }
    if (a->methodSymbol->onwer == nullptr) {
      Error::internal("owner is null");
    }
    if (a->methodSymbol->onwer->memberScope == nullptr) {
      Error::internal("memberscope is null");
    }

    auto scope = a->methodSymbol->onwer->memberScope;
    auto it_ = scope->method.find(a->name);
    if (it_ == a->methodSymbol->onwer->memberScope->method.end()) {
      Error::internal("cannot find method");
    }
    symbol->target->memberScope->method.emplace(a->name,
                                                std::move(it_->second));
    a->methodSymbol->onwer = symbol->target;
  }

  symbol->memberScope->method.clear();

  currentSelf = prevSelfScope;
}

void Resolver::visit(TraitDecl *decl) {
  ScopeGuard _(*table, decl->symbol->memberScope);
  for (auto a : decl->traitSigs) {
    a->accept(this);
  }
}

void Resolver::visit(FuncDecl *decl) {

  if (decl->returnType.has_value()) {
    decl->returnType->get()->accept(this);
    decl->methodSymbol->returnType = decl->returnType->get()->resolved;
  }

  for (auto p : decl->params) {
    p->accept(this);
    p->symbol->typeSymbol = p->type->resolved;
  }
  MethodSymbol *prev = currentMethod;
  currentMethod = decl->methodSymbol;
  ScopeGuard _(*table, decl->methodSymbol->scope);
  decl->body->accept(this);

  auto symbol = decl->methodSymbol;

  if (decl->returnType.has_value()) {
    auto rt = decl->returnType.value().get();
    rt->accept(this);
    auto type = rt->resolved;
    for (auto r : symbol->returns) {
      if (!isAssignable(type, r->resolved)) {
        Error::diagnostic(r->token, "unmatched return type");
      }
    }
  } else {
    if (symbol->returns.empty()) {
      symbol->returnType = table->getType("void");
    } else {
      auto rt = symbol->returns[0]->resolved;
      for (auto r : symbol->returns) {
        if (!isAssignable(rt, r->resolved)) {
          Error::diagnostic(r->token, "unmatched return type");
        }
      }
      symbol->returnType = rt;
    }
  }

  currentMethod = prev;
}
void Resolver::visit(VarDecl *decl) {
  decl->type->accept(this);
  decl->symbol->typeSymbol = decl->type->resolved;
}
void Resolver::visit(ArrayDecl *decl) {
  decl->type->accept(this);
  decl->symbol->typeSymbol = decl->type->resolved;
  decl->baseType = decl->type->elementType->resolved;
}

void Resolver::visit(TypeNode *type) {
  auto symbol = table->getType(type->type);
  if (!symbol)
    Error::diagnostic(type->token, "unknown type");
  type->resolved = symbol;
}
void Resolver::visit(ASTNode *) {}

void Resolver::visit(TraitSig *sig) {
  sig->type->accept(this);
  for (auto p : sig->params) {
    p->accept(this);
    p->symbol->typeSymbol = p->type->resolved;
  }
}
void Resolver::visit(Param *param) { param->type->accept(this); }

ValueSymbol *Resolver::resolveValue(str name) {
  if (auto v = table->getValue(name))
    return v;
  if (currentSelf && currentSelf->value.find(name) != currentSelf->value.end())
    return currentSelf->value.find(name)->second.get();

  return nullptr;
}

ValueSymbol *Resolver::lookLocalValue(str name, Scope *localScope) {
  if (localScope->value.find(name) != localScope->value.end())
    return localScope->value.find(name)->second.get();
  return nullptr;
}

bool Resolver::isAssignable(TypeSymbol *from, TypeSymbol *to) {
  if (from == to)
    return true;

  return false;
}