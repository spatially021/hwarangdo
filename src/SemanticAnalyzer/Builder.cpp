#include "SemanticAnalyzer/Builder.h"
#include "AST/ASTNode.h"
#include "AST/Expr.h"
#include "SemanticAnalyzer/Guard.h"
#include "SemanticAnalyzer/Symbol.h"
#include "util/Error.h"
#include <memory>

Builder::Builder(SymbolTable *symbol) : table(symbol) {
  topLevel = make_unique<TypeSymbol>();
  topLevel->name = "<top-level>";
  currentType = topLevel.get();
}

void Builder::visit(LiteralExpr *) {}
void Builder::visit(BinaryExpr *) {}
void Builder::visit(NameExpr *) {}
void Builder::visit(UnaryExpr *) {}
void Builder::visit(CallExpr *) {}
void Builder::visit(AssignExpr *) {}
void Builder::visit(MemberExpr *) {}
void Builder::visit(ArrayAccessExpr *) {}
void Builder::visit(TernaryExpr *) {}
void Builder::visit(ThisExpr *) {}
void Builder::visit(SuperExpr *) {}
void Builder::visit(MoveExpr *) {}
void Builder::visit(BorrowExpr *) {}
void Builder::visit(ReferenceExpr *) {}

// Statement Builder::visitor methods
void Builder::visit(ExprStmt *) {}
void Builder::visit(BlockStmt *stmt) {
  ScopeGuard _(*table);
  stmt->blockScope = table->getCurrent();
  for (auto s : stmt->statements) {
    s->accept(this);
  }
}
void Builder::visit(IfStmt *stmt) {
  stmt->thenBranch->accept(this);
  if (stmt->elseBranch != nullptr)
    stmt->elseBranch->accept(this);
}
void Builder::visit(ForStmt *stmt) {
  stmt->initializer->accept(this);
  ScopeGuard _(*table);
  stmt->body->accept(this);
}

void Builder::visit(WhileStmt *stmt) { stmt->body->accept(this); }

void Builder::visit(SwitchStmt *stmt) {
  for (auto c : stmt->clauses) {
    c->accept(this);
  }
}
void Builder::visit(Case *stmt) { stmt->body->accept(this); }

void Builder::visit(ReturnStmt *) {}
void Builder::visit(BreakStmt *) {}
void Builder::visit(ContinueStmt *) {}

void Builder::visit(DeclStmt *stmt) { stmt->decl->accept(this); }

void Builder::visit(EmptyStmt *) {}

void Builder::visit(ClassDecl *decl) {
  auto symbol = make_unique<TypeSymbol>();
  symbol->name = decl->name;
  symbol->decl = decl;
  symbol->kind = TypeSymbol::Kind::CLASS;
  symbol->baseName = decl->baseClass;
  auto raw = symbol.get();

  if (!table->add(std::move(symbol))) {
    Error::diagnostic(decl->token, "duplicated class name");
  }

  decl->symbol = raw;

  TypeContextGuard _(currentType, raw);
  ScopeGuard __(*table);

  raw->memberScope = table->getCurrent();

  for (auto a : decl->body) {
    a->accept(this);
  }
}

void Builder::visit(StructDecl *decl) {
  auto symbol = make_unique<TypeSymbol>();
  symbol->name = decl->name;
  symbol->decl = decl;
  symbol->kind = TypeSymbol::Kind::STRUCT;
  auto raw = symbol.get();

  if (!table->add(std::move(symbol))) {
    Error::diagnostic(decl->token, "duplicated struct name");
  }

  decl->symbol = raw;

  TypeContextGuard _(currentType, raw);
  ScopeGuard __(*table);

  raw->memberScope = table->getCurrent();
  for (auto a : decl->fields) {
    a->accept(this);
  }
}

void Builder::visit(EnumDecl *decl) {

  auto symbol = make_unique<TypeSymbol>();
  symbol->name = decl->name;
  symbol->decl = decl;
  symbol->kind = TypeSymbol::Kind::ENUM;

  auto raw = symbol.get();

  if (!table->add(std::move(symbol))) {
    Error::diagnostic(decl->token, "duplicated enum name");
  }

  decl->symbol = raw;

  TypeContextGuard _(currentType, raw);

  int ordinal = 0;
  for (auto a : decl->variants) {
    auto v = make_unique<EnumVariantSymbol>();
    v->name = a->name;
    v->ordinal = ordinal++;
    if (a->payload.has_value()) {
      a->payload.value()->accept(this);
      v->payloadType = a->payload.value()->resolved;
    }

    if (raw->variantMap.count(v->name)) {
      Error::diagnostic(a->token, "duplicated enum variant name");
    }
    EnumVariantSymbol *r = v.get();
    decl->symbol->variants.push_back(std::move(v));
    decl->symbol->variantMap.emplace(r->name, r);
  }
}

void Builder::visit(ImplDecl *decl) {
  auto symbol = make_unique<ImplSymbol>();
  symbol->targetName = decl->target;
  symbol->decl = decl;

  auto raw = symbol.get();

  ScopeGuard _(*table);
  TypeContextGuard __(currentType, raw);

  symbol->memberScope = table->getCurrent();
  for (auto a : decl->LinkedImplMethods) {
    a->accept(this);
  }

  table->impls.push_back(std::move(symbol));
  table->implMap.emplace(decl, raw);
}

void Builder::visit(TraitDecl *decl) {
  auto symbol = make_unique<TypeSymbol>();
  symbol->name = decl->name;
  symbol->decl = decl;
  symbol->kind = TypeSymbol::Kind::TRAIT;
  auto raw = symbol.get();

  if (!table->add(std::move(symbol))) {
    Error::diagnostic(decl->token, "duplicated trait name");
  }

  decl->symbol = raw;

  TypeContextGuard _(currentType, raw);
  ScopeGuard __(*table);

  raw->memberScope = table->getCurrent();

  for (auto a : decl->traitSigs) {
    if (a == nullptr)
      Error::internal("traitSig is nullptr");
    a->accept(this);
  }
}

void Builder::visit(TraitSig *sig) {

  auto symbol = make_unique<MethodSymbol>();

  symbol->name = sig->name;
  symbol->decl = sig;
  symbol->onwer = currentType;
  auto raw = symbol.get();

  ScopeGuard _(*table);

  for (auto a : sig->params) {
    auto s = make_unique<ValueSymbol>();
    s->name = a->name;
    s->kind = ValueSymbol::Kind::PARAM;
    s->node = a.get();
    auto r = s.get();
    if (!table->add(std::move(s))) {
      Error::diagnostic(a->token, "duplicated parameter name");
    }
    a->symbol = r;
    a->type->accept(this);
  }

  if (!table->add(std::move(symbol))) {
    Error::diagnostic(sig->token, "duplicated trait's method name");
  }

  sig->symbol = raw;
}

void Builder::visit(FuncDecl *decl) {
  auto symbol = make_unique<MethodSymbol>();

  symbol->name = decl->name;
  symbol->decl = decl;
  symbol->onwer = currentType;
  auto raw = symbol.get();

  if (!table->add(std::move(symbol))) {
    Error::diagnostic(decl->token, "duplicated method name");
  }

  decl->methodSymbol = raw;

  ScopeGuard _(*table);

  raw->scope = table->getCurrent();

  for (auto &a : decl->params) {
    auto s = make_unique<ValueSymbol>();
    s->name = a->name;
    s->kind = ValueSymbol::Kind::PARAM;
    s->node = a.get();
    auto r = s.get();
    if (!table->add(std::move(s))) {
      Error::diagnostic(a->token, "duplicated parameter name");
    }
    a->symbol = r;
    a->type->accept(this);
  }

  decl->body->accept(this);
}

void Builder::visit(VarDecl *decl) {
  auto symbol = make_unique<ValueSymbol>();
  symbol->name = decl->name;
  symbol->kind = ValueSymbol::Kind::VAR;
  symbol->node = decl;
  auto raw = symbol.get();

  decl->type->accept(this);

  if (!table->add(std::move(symbol))) {
    Error::diagnostic(decl->token, "duplicated var name");
  }
  decl->symbol = raw;
}

void Builder::visit(ArrayDecl *decl) {
  auto symbol = make_unique<ValueSymbol>();
  symbol->name = decl->name;
  symbol->kind = ValueSymbol::Kind::VAR;
  symbol->node = decl;
  auto raw = symbol.get();

  if (!table->add(std::move(symbol))) {
    Error::diagnostic(decl->token, "duplicated var name");
  }
  decl->symbol = raw;
}

void Builder::visit(TypeNode *type) {
  auto temp = dynamic_cast<BuiltinTypeNode *>(type);
  if (temp) {
  }
}
void Builder::visit(ASTNode *) {}
void Builder::visit(Param *) {}