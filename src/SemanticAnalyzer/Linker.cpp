#include "SemanticAnalyzer/Linker.h"
#include "AST/Decl.h"
#include "AST/Expr.h"
#include "AST/Stmt.h"
#include "SemanticAnalyzer/Scope.h"
#include "SemanticAnalyzer/SymbolTable.h"
#include "SemanticAnalyzer/symbol/MethodSymbol.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "SemanticAnalyzer/symbol/ValueSymbol.h"
#include "util/Error.h"
#include "util/Guard.h"
#include <cassert>
#include <memory>
#include <utility>
#include <vector>

Linker::Linker(SymbolTable *t) : table(t) {}

void Linker::visit(LiteralExpr *) {}
void Linker::visit(BinaryExpr *expr) {
  expr->left->accept(this);
  expr->right->accept(this);
}
void Linker::visit(NameExpr *) {}
void Linker::visit(UnaryExpr *expr) { expr->right->accept(this); }
void Linker::visit(CallExpr *expr) {
  expr->receiver->accept(this);
  for (auto &a : expr->arguments) {
    a->accept(this);
  }
}
void Linker::visit(AssignExpr *expr) {
  expr->target->accept(this);
  expr->value->accept(this);
}
void Linker::visit(MemberExpr *expr) { expr->object->accept(this); }
void Linker::visit(ArrayAccessExpr *expr) {
  expr->object->accept(this);
  expr->index->accept(this);
}
void Linker::visit(TernaryExpr *expr) {
  expr->conditon->accept(this);
  expr->then->accept(this);
  expr->else_->accept(this);
}
void Linker::visit(ThisExpr *) {}
void Linker::visit(SuperExpr *) {}
void Linker::visit(CastExpr *) {}
void Linker::visit(BuiltInNameExpr *) {}
void Linker::visit(SpawnExpr *expr) {
  expr->left->accept(this);
  expr->spawnType->accept(this);
}
void Linker::visit(ViewExpr *expr) {
  expr->left->accept(this);
  expr->target->accept(this);
}
void Linker::visit(DefaultValueExpr *) {}
void Linker::visit(Range *) {}
void Linker::visit(CaseValueExpr *expr) {
  expr->value->accept(this);
  if (expr->arg) {
    expr->arg->accept(this);
  }
}
void Linker::visit(MatchExpr *) {}
// Statement Linker::visitor methods
void Linker::visit(ExprStmt *stmt) { stmt->expr->accept(this); }
void Linker::visit(BlockStmt *stmt) {
  ScopeGuard _(*table, stmt->blockScope);
  for (auto s : stmt->statements) {
    s->accept(this);
  }
}
void Linker::visit(IfStmt *stmt) {
  stmt->thenBranch->accept(this);
  if (stmt->elseBranch != nullptr) {
    stmt->elseBranch->accept(this);
  }
}
void Linker::visit(ForStmt *stmt) {
  stmt->initializer->accept(this);
  stmt->range->accept(this);
  stmt->body->accept(this);
}
void Linker::visit(WhileStmt *stmt) { stmt->body->accept(this); }
void Linker::visit(SwitchStmt *stmt) {
  for (auto &c : stmt->clauses) {
    c->accept(this);
  }
}
void Linker::visit(Case *c) { c->body->accept(this); }
void Linker::visit(ReturnStmt *stmt) { stmt->value->accept(this); }
void Linker::visit(ValueTransferStmt *stmt) { stmt->value->accept(this); }
void Linker::visit(BreakStmt *) {}
void Linker::visit(ContinueStmt *) {}
void Linker::visit(DeclStmt *stmt) { stmt->decl->accept(this); }
void Linker::visit(EmptyStmt *) {}

// declare Linker::visitor methods
void Linker::visit(ClassDecl *decl) {

  if (decl->symbol->type == Symbol::SymbolType::MAIN) {
    auto symbol = static_cast<MainSymbol *>(decl->symbol);
    auto it = symbol->memberScope->method.find("update");
    if (it == symbol->memberScope->method.end()) {
      Error::diagnostic(decl->token, "has no update method");
    }
    symbol->main = it->second.get();
  }

  if (decl->baseClass.has_value()) {
    string s = decl->baseClass.value();
    if (table->isType(s)) {
      auto symbol = table->getType(s);
      symbol->decl->isExtended = true;
      if (symbol->kind != TypeSymbol::TypeKind::CLASS) {
        Error::diagnostic(decl->token, s + " is not class");
      }
    } else {
      Error::diagnostic(decl->token, "unknown parent class '" + s + "'");
    }
  }
  for (auto &t : decl->traits) {
    if (!table->isType(t)) {
      Error::diagnostic(decl->token, "unknown trait '" + t + "'");
    }
    auto symbol = table->getType(t);
    if (symbol->kind != TypeSymbol::TypeKind::TRAIT) {
      Error::diagnostic(decl->token, t + " is not trait");
    }
    decl->symbol->traits.push_back(symbol);
  }

  ScopeGuard _(*table, decl->symbol->memberScope);
  TypeContextGuard __(currentType, decl->symbol);

  for (auto &a : decl->fields)
    a->accept(this);
  for (auto &a : decl->methods) {
    a->accept(this);
  }
  for (auto &a : decl->innerDecl) {
    a->accept(this);
  }

  for (auto &t : decl->symbol->traits) {
    auto trait = dynamic_cast<TraitDecl *>(t->decl);
    if (!trait) {
      Error::internal(decl->token, "unmatded decl subClass : " + t->decl->name);
    }
    for (auto &s : trait->traitSigs) {
      auto it = decl->symbol->methodsName.find(s->name);
      if (it == decl->symbol->methodsName.end()) {
        Error::diagnostic(decl->token, "undeclared trait sig : " + s->name);
      }
    }
  }
}

void Linker::visit(StructDecl *decl) {
  ScopeGuard _(*table, decl->symbol->memberScope);
  for (auto &f : decl->fields) {
    f->accept(this);
  }
}
void Linker::visit(EnumDecl *decl) {
  for (auto &v : decl->variants) {
    if (v->payload.has_value()) {
      auto t = v->payload.value().get();
      auto s = table->getType(t);
      if (!s)
        Error::diagnostic(t->token, "unknown type : " + t->token.text);
      t->resolved = s;
      decl->symbol->variantMap[v->name]->payloadType = table->getType(t);
    }
  }
}
void Linker::visit(ImplDecl *decl) {
  ScopeGuard _(*table, table->implMap[decl]->memberScope);
  string s = decl->target;
  if (!table->isType(s)) {
    Error::diagnostic(decl->token, "unknown impl target");
  }
  auto symbol = table->getType(s);
  if (symbol->kind != TypeSymbol::TypeKind::STRUCT) {
    Error::diagnostic(decl->token, s + " is not struct");
  }

  for (auto &m : decl->LinkedImplMethods) {
    auto it = symbol->methodsName.find(m->name);
    if (it == symbol->methodsName.end()) {
      symbol->methodsName.emplace(m->name, decl->token);
    } else {
      Error::diagnostic(m->token,
                        "duplicate impl method '" + m->name + "' for struct '" +
                            s + "'",
                        it->second, "previous impl method declared here");
    }
  }
  for (auto &a : decl->LinkedImplMethods) {
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

    auto it = table->implMap.find(decl);
    if (it == table->implMap.end()) {
      Error::internal("this impl is not declared");
    }
    ScopeGuard __(*table, symbol->memberScope);

    ImplSymbol *impl = it->second;

    impl->target->memberScope->method.emplace(a->name, std::move(it_->second));
    a->methodSymbol->onwer = impl->target;
  }

  symbol->memberScope->method.clear();
}

void Linker::visit(TraitDecl *decl) {
  ScopeGuard _(*table, decl->symbol->memberScope);
  for (auto &s : decl->traitSigs) {
    s->accept(this);
  }
}

void Linker::visit(FuncDecl *decl) {
  if (decl->returnType.has_value()) {
    decl->returnType->get()->accept(this);
    decl->methodSymbol->returnType = decl->returnType->get()->resolved;
  }

  ScopeGuard _(*table, decl->methodSymbol->scope);
  for (auto &p : decl->params) {
    p->accept(this);
    p->symbol->typeSymbol = p->type->resolved;
  }

  if (decl->isOverride) {
    if (currentType->base->memberScope->method.find(decl->name) ==
        currentType->base->memberScope->method.end()) {
      Error::diagnostic(decl->token, "unkwown override target : " + decl->name);
    }
  }

  if (decl->isFrame) {
    if (!dynamic_cast<MainSymbol *>(currentType)) {
      Error::diagnostic(decl->token, "frame can only in Main class");
    }
    if (decl->name != "update") {
      Error::diagnostic(decl->token, "after frame need method name - update");
    }
  }

  decl->body->accept(this);
}
void Linker::visit(VarDecl *decl) {
  if (decl->init) {
    decl->init->accept(this);
  }
  decl->type->accept(this);

  decl->symbol->typeSymbol = decl->type->resolved;
  decl->symbol->typeSymbol = decl->type->resolved;

  if (!decl->type->resolved) {
    Error::internal(decl->token, "decl->type->resolved is nullptr");
  }
  if (!decl->symbol->typeSymbol) {
    Error::internal(decl->token, "typeSymbol is nullptr");
  }
}

void Linker::visit(ArrayDecl *decl) {
  auto type = table->getType(decl->type->elementType->type);
  if (!type) {
    Error::internal(decl->token, "fail to get type : " + decl->type->type);
  }

  if (table->getCurrent()->scopeKind == Scope::ScopeKind::FIELD) {
    if (!isDeclField(type)) {
      Error::diagnostic(decl->token,
                        "invalid field type '" + decl->type->type +
                            "' (only primitive or handle types are allowed)");
    }
  }
  decl->type->accept(this);
  decl->type->elementType->accept(this);
  decl->symbol->typeSymbol = decl->type->resolved;
  if (decl->init) {
    decl->init->accept(this);
  }
}

void Linker::visit(TypeNode *type) {
  if (dynamic_cast<BuiltinTypeNode *>(type) ||
      dynamic_cast<IdentifierTypeNode *>(type)) {
    auto symbol = table->getType(type);
    if (!symbol)
      Error::diagnostic(type->token, "unknown type : " + type->token.text);
    type->resolved = symbol;
  } else if (auto a = dynamic_cast<ArrayTypeNode *>(type)) {
    a->elementType->accept(this);
    if (!a->elementType->resolved) {
      Error::internal(a->token, "array element type nullptr : " +
                                    a->elementType->token.text);
    }
    a->resolved = a->elementType->resolved;
  } else if (auto g = dynamic_cast<GenericTypeNode *>(type)) {
    auto ar = g->typeArgs;
    TypeSymbol *orign = nullptr;
    vector<TypeSymbol *> args;
    for (auto &t : g->typeArgs) {
      t->accept(this);
      if (!t->resolved) {
        Error::internal(t->token,
                        "fail to resolve args Type : " + t->token.text);
      }
      args.push_back(t->resolved);
    }

    switch (g->gKind) {
    case GenericTypeNode::GenericKind::HANDLE:

      if (args.size() != 1) {
        Error::diagnostic(g->token, "Handle need one type but '" +
                                        to_string(args.size()) + "'");
      }

      if (ar[0]->resolved->kind == TypeSymbol::TypeKind::PRIMITIVE) {
        Error::diagnostic(ar[0]->token, "not allowed handle target type : " +
                                            ar[0]->token.text);
      }
      if (ar[0]->resolved->type == Symbol::SymbolType::MAIN) {
        Error::diagnostic(ar[0]->token, "not allowed handle target type : " +
                                            ar[0]->token.text);
      }

      orign = table->getHandle();
      break;
    case GenericTypeNode::GenericKind::OPTION:
      if (args.size() != 1) {
        Error::diagnostic(g->token, "Option need one type but '" +
                                        to_string(args.size()) + "'");
      }
      orign = table->getOption();
      break;
    case GenericTypeNode::GenericKind::RESULT:
      if (args.size() != 2) {
        Error::diagnostic(g->token, "Result neet two type but '" +
                                        to_string(args.size()) + "'");
      }
      if (args[1]->kind != TypeSymbol::TypeKind::ERROR) {
        Error::diagnostic(g->token, "Result's second type is Error but '" +
                                        args[1]->name);
      }
      break;
    }
    g->resolved = table->GenericInsGetOrCreate(orign, args);
  } else {
    Error::diagnostic(type->token, "unknown type : " + type->token.text);
  }
}
void Linker::visit(ASTNode *) {}

void Linker::visit(TraitSig *sig) {
  for (auto &p : sig->params) {
    p->accept(this);
  }
  sig->type->accept(this);
}
void Linker::visit(Param *param) {
  param->type->accept(this);
  param->symbol->typeSymbol = param->type->resolved;
  if (!param->symbol->typeSymbol) {
    Error::internal(param->token, "param type is unlinked");
  }
}

void Linker::visit(InitDecl *decl) {
  ScopeGuard _(*table, decl->methodSymbol->scope);

  for (auto &p : decl->params) {
    p->accept(this);
    p->symbol->typeSymbol = p->type->resolved;
  }
  decl->body->accept(this);
}