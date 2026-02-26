#include "SemanticAnalyzer/Resolver.h"
#include "AST/ASTNode.h"
#include "AST/Decl.h"
#include "AST/Expr.h"
#include "AST/Stmt.h"
#include "IR/MIR.h"
#include "SemanticAnalyzer/Guard.h"
#include "SemanticAnalyzer/Scope.h"
#include "SemanticAnalyzer/Symbol.h"
#include "SemanticAnalyzer/SymbolTable.h"
#include "util/Error.h"
#include <cassert>
#include <cstddef>
#include <memory>
#include <string>

Resolver::Resolver(SymbolTable *t) : table(t) {}

void Resolver::visit(LiteralExpr *expr) {
  switch (expr->token.kind) {
  case TKind::LIT_INT:
    expr->resolvedType = table->getType("i32");
    break;
  case TKind::LIT_FLOAT:
    expr->resolvedType = table->getType("f32");
    break;
  case TKind::FIXED:
    expr->resolvedType = table->getType("fi16");
    break;
  case TKind::LIT_CHARACTER:
    expr->resolvedType = table->getType("c8");
    break;
  case TKind::LIT_STRING:
    expr->resolvedType = table->getType("string8");
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

  if (isBinaryOperatalbe(expr->op, expr->left->resolvedType,
                         expr->right->resolvedType)) {
    expr->resolvedType = binaryResult(expr->op, expr->left->resolvedType,
                                      expr->right->resolvedType);
  } else {
    Error::diagnostic(expr->token, "leftExpr and rightExpr cannot operate.");
  }
}

void Resolver::visit(NameExpr *expr) {
  auto symbol = resolveValue(expr->name);
  if (!symbol) {
    auto temp = table->getType(expr->name);
    if (temp) {
      if (temp->kind == TypeSymbol::Kind::ENUM) {
        expr->resolvedType = temp;
        expr->typeSymbol = temp;
        return;
      } else {
        Error::internal("expect enum");
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

void Resolver::ResolveEnumVariant(CallExpr *expr) {

  auto variant = static_cast<EnumVariantSymbol *>(expr->resolved);
  if (!variant) {
    Error::internal("casting fail enum variant");
  }

  if (expr->arguments.size() > 1)
    Error::diagnostic(expr->token, "in variant only one payload allowed");

  if (expr->arguments.size() == 1) {
    expr->arguments[0]->accept(this);
    if (variant->payloadType == nullptr)
      Error::diagnostic(expr->token, "this variant does not take payload");
    else if (!isAssignable(variant->payloadType,
                           expr->arguments[0]->resolvedType))
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

  auto method = static_cast<MethodSymbol *>(expr->resolved);
  auto funcDecl = static_cast<FuncDecl *>(method->decl);

  if (expr->arguments.size() != funcDecl->params.size())
    Error::diagnostic(expr->token, "mismatch argument count");

  for (size_t i = 0; i < expr->arguments.size(); ++i) {

    auto symbol = static_cast<TypeSymbol *>(expr->arguments[i]->resolvedType);
    if (!symbol) {
      unmatchSymbol(symbol);
    }
    if (!isAssignable(symbol, funcDecl->params[i]->symbol->typeSymbol)) {
      Error::diagnostic(expr->token, "unmatched argument type");
    }
    if (funcDecl->params[i]->isBorrow &&
        expr->arguments[i]->kind != NKind::BORROW_EXPR) {
    }
  }

  expr->resolved = method->returnType;
}

void Resolver::visit(CallExpr *expr) {
  if (expr->callType == CallExpr::CallType::UNRESOLVED) {
    if (expr->receiver != nullptr) {
      expr->receiver->accept(this);
      auto re = dynamic_cast<TypeSymbol *>(expr->receiver->resolvedType);
      if (!re) {
        unmatchSymbol(expr->receiver->resolvedType);
      }

      if (re->kind == TypeSymbol::Kind::ENUM) {
        auto it = re->variantMap.find(expr->methodName);
        if (it == re->variantMap.end()) {
          Error::diagnostic(expr->token, "unknown variant name");
        }
        expr->callType = CallExpr::CallType::PAYLOAD_CALL;
        expr->resolved = static_cast<EnumVariantSymbol *>(it->second);

        ResolveEnumVariant(expr);
      } else {
        expr->callType = CallExpr::CallType::FUNC_CALL;

        auto scope = expr->receiver->resolvedType->memberScope;
        if (!scope) {
          Error::diagnostic(expr->token, "type has no members");
        }

        auto it = scope->method.find(expr->methodName);
        if (it == scope->method.end()) {
          Error::diagnostic(expr->token,
                            "unknwon method name : " + expr->methodName);
        }

        expr->resolved = static_cast<MethodSymbol *>(it->second.get());
        ResolveCall(expr);
      }
    } else {
    }
  }
}

void Resolver::visit(AssignExpr *expr) {
  expr->target->accept(this);
  expr->value->accept(this);
  if (!isAssignable(expr->target->resolvedType, expr->value->resolvedType)) {
    Error::diagnostic(expr->token, "unmatched assign type");
  }
}
void Resolver::visit(MemberExpr *expr) {
  expr->object->accept(this);

  auto symbol = static_cast<TypeSymbol *>(expr->object->resolvedType);

  if (symbol->kind == TypeSymbol::Kind::ENUM) {
    auto it = symbol->variantMap.find(expr->member);
    if (it == symbol->variantMap.end()) {
      Error::diagnostic(expr->token, expr->member + " is not " +
                                         expr->object->resolvedType->name +
                                         "'s variant");
    }
    expr->resolved = it->second;
  } else {
    auto s = static_cast<TypeSymbol *>(expr->object->resolvedType);
    auto scope = s->memberScope;
    if (!scope)
      Error::diagnostic(expr->token, "type has no members");

    auto it = scope->value.find(expr->member);
    if (it == scope->value.end())
      Error::diagnostic(expr->token,
                        "undeclared member '" + expr->member + "'");

    auto member = it->second.get();
    if (!member)
      Error::diagnostic(expr->token, "member '" + expr->member + "' is null");

    expr->resolved = member;
  }
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

void Resolver::visit(MoveExpr *expr) {
  if (expr->target->kind == NKind::VAR_EXPR) {
    expr->target->accept(this);
    expr->resolvedType = expr->target->resolvedType;
  } else {
    Error::diagnostic(expr->token, "ownership move can only variable");
  }
}
void Resolver::visit(BorrowExpr *expr) {
  expr->target->accept(this);
  expr->resolvedType = expr->target->resolvedType;
}
void Resolver::visit(ReferenceExpr *expr) {
  if (expr->target->kind == NKind::VAR_EXPR) {
    expr->target->accept(this);
    expr->resolvedType = expr->target->resolvedType;
  } else {
    Error::diagnostic(expr->token, "reference can only variable");
  }
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
  if (!table->isBool(stmt->condition->resolvedType)) {
    Error::diagnostic(stmt->condition->token, "condition is not bool type");
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
  if (!table->isBool(stmt->condition->resolvedType))
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
    stmt->returnType = static_cast<TypeSymbol *>(stmt->value->resolvedType);
  } else {
    stmt->returnType = table->getType("void");
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
    symbol->isInhereted = true;
    symbol->base->isInhereted = true;
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
      if (!isAssignable(type, r->returnType)) {
        Error::diagnostic(r->token, "unmatched return type");
      }
    }
  } else {
    if (symbol->returns.empty()) {
      symbol->returnType = table->getType("void");
    } else {
      auto rt = symbol->returns[0]->returnType;
      for (auto r : symbol->returns) {
        if (!isAssignable(rt, r->returnType)) {
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

bool Resolver::isBinaryOperatalbe(BinaryExpr::OperatorType op, TypeSymbol *left,
                                  TypeSymbol *right) {

  assert(left != nullptr);
  assert(right != nullptr);

  switch (op) {
  case BinaryExpr::OperatorType::B_AND:
  case BinaryExpr::OperatorType::B_OR:
  case BinaryExpr::OperatorType::B_XOR:
  case BinaryExpr::OperatorType::LSH:
  case BinaryExpr::OperatorType::RSH:
    return table->isInt(left) && table->isInt(right);

  case BinaryExpr::OperatorType::LS:
  case BinaryExpr::OperatorType::LSE:
  case BinaryExpr::OperatorType::GR:
  case BinaryExpr::OperatorType::GRE:
  case BinaryExpr::OperatorType::ADD:
  case BinaryExpr::OperatorType::SUB:
  case BinaryExpr::OperatorType::MUL:
  case BinaryExpr::OperatorType::DIV:
  case BinaryExpr::OperatorType::REM:
  case BinaryExpr::OperatorType::POW:
    return table->isNumberic(left) && table->isNumberic(right);

  case BinaryExpr::OperatorType::AND:
  case BinaryExpr::OperatorType::OR:
    return table->isBool(left) && table->isBool(right);

  case BinaryExpr::OperatorType::EQ:
  case BinaryExpr::OperatorType::NT:
    return isCmpable(left, right);
    break;
  }
  return false;
}

bool Resolver::isCmpable(TypeSymbol *left, TypeSymbol *right) {

  if (table->isNumberic(left) && table->isNumberic(right))
    return true;

  if (table->isBool(left) && table->isBool(right))
    return true;

  if (left->kind == TypeSymbol::Kind::ENUM &&
      right->kind == TypeSymbol::Kind::ENUM)
    return left == right;

  return left == right;
}

TypeSymbol *Resolver::binaryResult(BinaryExpr::OperatorType op,
                                   TypeSymbol *left, TypeSymbol *right) {
  assert(left != nullptr);
  assert(right != nullptr);

  switch (op) {
  case BinaryExpr::OperatorType::B_AND:
  case BinaryExpr::OperatorType::B_OR:
  case BinaryExpr::OperatorType::B_XOR:
  case BinaryExpr::OperatorType::LSH:
  case BinaryExpr::OperatorType::RSH:
  case BinaryExpr::OperatorType::LS:
  case BinaryExpr::OperatorType::LSE:
  case BinaryExpr::OperatorType::GR:
  case BinaryExpr::OperatorType::GRE:
  case BinaryExpr::OperatorType::ADD:
  case BinaryExpr::OperatorType::SUB:
  case BinaryExpr::OperatorType::MUL:
  case BinaryExpr::OperatorType::DIV:
  case BinaryExpr::OperatorType::REM:
  case BinaryExpr::OperatorType::POW:
    return casting(left, right);

  case BinaryExpr::OperatorType::AND:
  case BinaryExpr::OperatorType::OR:
  case BinaryExpr::OperatorType::EQ:
  case BinaryExpr::OperatorType::NT:
    return table->getType("bool");
    break;
  }
  return nullptr;
}

[[noreturn]]
void Resolver::unmatchSymbol(Symbol *symbol) {
  Error::internal(symbol->name + ": unmatched symbol");
}

bool Resolver::isCastable(TypeSymbol *from, TypeSymbol *to) {
  return from == to;
}
TypeSymbol *Resolver::casting(TypeSymbol *left, TypeSymbol *) { return left; }