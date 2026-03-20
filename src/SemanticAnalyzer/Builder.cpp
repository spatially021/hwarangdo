#include "SemanticAnalyzer/Builder.h"
#include "AST/ASTNode.h"
#include "AST/Decl.h"
#include "AST/Expr.h"
#include "SemanticAnalyzer/Guard.h"
#include "SemanticAnalyzer/Scope.h"
#include "SemanticAnalyzer/symbol/MethodSymbol.h"
#include "SemanticAnalyzer/symbol/ValueSymbol.h"
#include "util/Error.h"
#include <memory>
#include <utility>

Builder::Builder(SymbolTable *symbol) : table(symbol) {
  topLevel = make_unique<TypeSymbol>();
  topLevel->name = "<top-level>";
  currentType = topLevel.get();
  rootScope->id = -1;
}

void Builder::visit(LiteralExpr *) {}
void Builder::visit(BinaryExpr *expr) {
  expr->left->accept(this);
  expr->right->accept(this);
}
void Builder::visit(NameExpr *) {}
void Builder::visit(UnaryExpr *expr) { expr->right->accept(this); }
void Builder::visit(CallExpr *expr) {
  expr->receiver->accept(this);
  for (auto a : expr->arguments) {
    a->accept(this);
  }
}
void Builder::visit(AssignExpr *expr) {
  expr->value->accept(this);
  expr->target->accept(this);
}
void Builder::visit(MemberExpr *expr) { expr->object->accept(this); }
void Builder::visit(ArrayAccessExpr *expr) {
  expr->object->accept(this);
  expr->index->accept(this);
}
void Builder::visit(TernaryExpr *expr) {
  expr->conditon->accept(this);
  expr->then->accept(this);
  if (expr->else_) {
    expr->else_->accept(this);
  }
}
void Builder::visit(ThisExpr *) {}
void Builder::visit(SuperExpr *) {}
void Builder::visit(CastExpr *expr) {
  expr->left->accept(this);
  expr->type->accept(this);
}
void Builder::visit(BuiltInNameExpr *) {}
void Builder::visit(SpawnExpr *expr) {
  expr->left->accept(this);
  for (auto &a : expr->args) {
    a->accept(this);
  }
}
void Builder::visit(ViewExpr *expr) {
  expr->left->accept(this);
  expr->target->accept(this);
}
void Builder::visit(DefaultValueExpr *) {}
void Builder::visit(Range *expr) {
  expr->from->accept(this);
  expr->to->accept(this);
}
void Builder::visit(CaseValueExpr *expr) {
  expr->value->accept(this);
  if (expr->arg) {
    expr->arg->accept(this);
  }
}
void Builder::visit(MatchExpr *expr) {
  ScopeGuard _(*table);
  table->getCurrent()->scopeKind = Scope::ScopeKind::BLOCK;
  expr->blockScope = table->getCurrent();
  for (auto &c : expr->cases) {
    c->accept(this);
  }
}
// Statement Builder::visitor methods
void Builder::visit(ExprStmt *) {}
void Builder::visit(BlockStmt *stmt) {
  ScopeGuard _(*table);
  stmt->blockScope = table->getCurrent();
  table->getCurrent()->scopeKind = Scope::ScopeKind::BLOCK;
  for (auto &s : stmt->statements) {
    s->accept(this);
  }
}
void Builder::visit(IfStmt *stmt) {
  stmt->thenBranch->accept(this);
  if (stmt->elseBranch != nullptr)
    stmt->elseBranch->accept(this);
}
void Builder::visit(ForStmt *stmt) {
  ScopeGuard _(*table);
  stmt->blockScope = table->getCurrent();
  stmt->initializer->accept(this);
  table->getCurrent()->scopeKind = Scope::ScopeKind::BLOCK;
  stmt->body->accept(this);
}

void Builder::visit(WhileStmt *stmt) { stmt->body->accept(this); }

void Builder::visit(SwitchStmt *stmt) {
  ScopeGuard _(*table);
  table->getCurrent()->scopeKind = Scope::ScopeKind::BLOCK;
  stmt->blockScope = table->getCurrent();

  for (auto &c : stmt->clauses) {
    c->accept(this);
  }
}
void Builder::visit(Case *stmt) { stmt->body->accept(this); }

void Builder::visit(ReturnStmt *) {}
void Builder::visit(ValueTransferStmt *) {}
void Builder::visit(BreakStmt *) {}
void Builder::visit(ContinueStmt *) {}

void Builder::visit(DeclStmt *stmt) { stmt->decl->accept(this); }

void Builder::visit(EmptyStmt *) {}

void Builder::buildMain(ClassDecl *decl) {

  auto symbol = make_unique<MainSymbol>();
  symbol->name = decl->name;
  symbol->decl = decl;

  auto raw = symbol.get();

  auto result = table->add(std::move(symbol));

  if (!result.success) {
    switch (result.errorType) {
    case SymbolTable::Result::DUPLICATED:
      Error::diagnostic(decl->token, "duplicated Main");
      break;
    case SymbolTable::Result::RESERVED:
      Error::diagnostic(decl->token, "reserved name : " + decl->name);
      break;
    case SymbolTable::Result::UNKNOWN_SYMBOL:
      Error::internal(decl->token, "unknown symbol" + decl->name);
      break;
    case SymbolTable::Result::NONE:
      break;
    }
  }
  decl->symbol = raw;
  table->main = raw;

  TypeContextGuard _(currentType, raw);
  ScopeGuard __(*table);

  raw->memberScope = table->getCurrent();
  table->getCurrent()->scopeKind = Scope::ScopeKind::FIELD;
  for (auto &a : decl->fields) {
    a->accept(this);
  }
  for (auto &a : decl->methods) {
    a->accept(this);
  }
  for (auto &a : decl->innterDecl) {
    a->accept(this);
  }
}

void Builder::visit(ClassDecl *decl) {
  if (decl->name == "Main") {
    buildMain(decl);
    return;
  }

  auto symbol = make_unique<TypeSymbol>();
  symbol->name = decl->name;
  symbol->decl = decl;
  symbol->kind = TypeSymbol::TypeKind::CLASS;
  symbol->baseName = decl->baseClass;
  auto raw = symbol.get();

  auto result = table->add(std::move(symbol));

  if (!result.success) {
    switch (result.errorType) {
    case SymbolTable::Result::DUPLICATED:
      Error::diagnostic(decl->token, "duplicated class name : " + decl->name);
      break;
    case SymbolTable::Result::RESERVED:
      Error::diagnostic(decl->token, "reserved name : " + decl->name);
      break;
    case SymbolTable::Result::UNKNOWN_SYMBOL:
      Error::internal(decl->token, "unknown symbol" + decl->name);
      break;
    case SymbolTable::Result::NONE:
      break;
    }
  }

  decl->symbol = raw;

  TypeContextGuard _(currentType, raw);
  ScopeGuard __(*table);
  table->getCurrent()->scopeKind = Scope::ScopeKind::FIELD;
  raw->memberScope = table->getCurrent();
  for (auto &a : decl->fields) {
    a->accept(this);
  }
  for (auto &a : decl->methods) {
    a->accept(this);
  }

  for (auto &a : decl->innterDecl) {
    if (canInnerDecl(a.get())) {
      a->accept(this);
    } else {
      Error::diagnostic(a->token,
                        "not allowed inner decl type : " + a->token.text);
    }
  }
}

void Builder::visit(StructDecl *decl) {
  auto symbol = make_unique<TypeSymbol>();
  symbol->name = decl->name;
  symbol->decl = decl;
  symbol->kind = TypeSymbol::TypeKind::STRUCT;
  auto raw = symbol.get();

  auto result = table->add(std::move(symbol));

  if (!result.success) {
    switch (result.errorType) {
    case SymbolTable::Result::DUPLICATED:
      Error::diagnostic(decl->token, "duplicated class name : " + decl->name);
      break;
    case SymbolTable::Result::RESERVED:
      Error::diagnostic(decl->token, "reserved name : " + decl->name);
      break;
    case SymbolTable::Result::UNKNOWN_SYMBOL:
      Error::internal(decl->token, "unknown symbol" + decl->name);
      break;
    case SymbolTable::Result::NONE:
      break;
    }
  }

  decl->symbol = raw;

  TypeContextGuard _(currentType, raw);
  ScopeGuard __(*table);
  table->getCurrent()->scopeKind = Scope::ScopeKind::FIELD;
  raw->memberScope = table->getCurrent();
  for (auto a : decl->fields) {
    a->accept(this);
  }
}

void Builder::visit(EnumDecl *decl) {

  auto symbol = make_unique<TypeSymbol>();
  symbol->name = decl->name;
  symbol->decl = decl;
  symbol->kind = TypeSymbol::TypeKind::ENUM;

  auto raw = symbol.get();

  auto result = table->add(std::move(symbol));

  if (!result.success) {
    switch (result.errorType) {
    case SymbolTable::Result::DUPLICATED:
      Error::diagnostic(decl->token, "duplicated class name : " + decl->name);
      break;
    case SymbolTable::Result::RESERVED:
      Error::diagnostic(decl->token, "reserved name : " + decl->name);
      break;
    case SymbolTable::Result::UNKNOWN_SYMBOL:
      Error::internal(decl->token, "unknown symbol" + decl->name);
      break;
    case SymbolTable::Result::NONE:
      break;
    }
  }

  decl->symbol = raw;

  TypeContextGuard _(currentType, raw);

  int ordinal = 0;
  for (auto a : decl->variants) {
    auto v = make_unique<EnumVariantSymbol>();
    v->name = a->name;
    v->ordinal = ordinal++;
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
  table->getCurrent()->scopeKind = Scope::ScopeKind::FIELD;
  symbol->memberScope = table->getCurrent();
  for (auto &a : decl->LinkedImplMethods) {
    a->accept(this);
  }

  table->impls.push_back(std::move(symbol));
  table->implMap.emplace(decl, raw);
}

void Builder::visit(TraitDecl *decl) {
  auto symbol = make_unique<TypeSymbol>();
  symbol->name = decl->name;
  symbol->decl = decl;
  symbol->kind = TypeSymbol::TypeKind::TRAIT;
  auto raw = symbol.get();

  auto result = table->add(std::move(symbol));

  if (!result.success) {
    switch (result.errorType) {
    case SymbolTable::Result::DUPLICATED:
      Error::diagnostic(decl->token, "duplicated trait name : " + decl->name);
      break;
    case SymbolTable::Result::RESERVED:
      Error::diagnostic(decl->token, "reserved name : " + decl->name);
      break;
    case SymbolTable::Result::UNKNOWN_SYMBOL:
      Error::internal(decl->token, "unknown symbol" + decl->name);
      break;
    case SymbolTable::Result::NONE:
      break;
    }
  }

  decl->symbol = raw;

  TypeContextGuard _(currentType, raw);
  ScopeGuard __(*table);
  table->getCurrent()->scopeKind = Scope::ScopeKind::FIELD;
  raw->memberScope = table->getCurrent();

  for (auto &a : decl->traitSigs) {
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

  auto result = table->add(std::move(symbol));

  if (!result.success) {
    switch (result.errorType) {
    case SymbolTable::Result::DUPLICATED:
      Error::diagnostic(sig->token, "duplicated class name : " + sig->name);
      break;
    case SymbolTable::Result::RESERVED:
      Error::diagnostic(sig->token, "reserved name : " + sig->name);
      break;
    case SymbolTable::Result::UNKNOWN_SYMBOL:
      Error::internal(sig->token, "unknown symbol" + sig->name);
      break;
    case SymbolTable::Result::NONE:
      break;
    }
  }
  ScopeGuard _(*table);
  table->getCurrent()->scopeKind = Scope::ScopeKind::FUNC;
  for (auto &a : sig->params) {
    auto s = make_unique<ValueSymbol>();
    s->name = a->name;
    s->kind = ValueSymbol::Kind::PARAM;
    s->node = a.get();
    auto r = s.get();

    auto re = table->add(std::move(s));

    if (!re.success) {
      switch (re.errorType) {
      case SymbolTable::Result::DUPLICATED:
        Error::diagnostic(a.get()->token,
                          "duplicated class name : " + a.get()->name);
        break;
      case SymbolTable::Result::RESERVED:
        Error::diagnostic(a.get()->token, "reserved name : " + a.get()->name);
        break;
      case SymbolTable::Result::UNKNOWN_SYMBOL:
        Error::internal(a.get()->token, "unknown symbol" + a.get()->name);
        break;
      case SymbolTable::Result::NONE:
        break;
      }
    }
    a->symbol = r;
    a->type->accept(this);
  }

  sig->symbol = raw;
}

void Builder::visit(FuncDecl *decl) {
  auto symbol = make_unique<MethodSymbol>();

  symbol->name = decl->name;
  symbol->decl = decl;
  symbol->onwer = currentType;
  auto raw = symbol.get();

  auto result = table->add(std::move(symbol));

  if (!result.success) {
    switch (result.errorType) {
    case SymbolTable::Result::DUPLICATED:
      Error::diagnostic(decl->token, "duplicated method name : " + decl->name);
      break;
    case SymbolTable::Result::RESERVED:
      Error::diagnostic(decl->token, "reserved name : " + decl->name);
      break;
    case SymbolTable::Result::UNKNOWN_SYMBOL:
      Error::internal(decl->token, "unknown symbol" + decl->name);
      break;
    case SymbolTable::Result::NONE:
      break;
    }
  }
  decl->methodSymbol = raw;

  ScopeGuard _(*table);

  raw->scope = table->getCurrent();
  raw->isExtern = decl->isExtern;
  raw->isFrame = decl->isFrame;
  raw->isOverride = decl->isOverride;

  for (auto &a : decl->params) {
    a->accept(this);
  }

  currentType->methodsName.emplace(raw->name, decl->token);

  decl->body->accept(this);
}

void Builder::visit(VarDecl *decl) {
  auto symbol = make_unique<ValueSymbol>();
  symbol->name = decl->name;
  symbol->kind = ValueSymbol::Kind::VAR;
  symbol->node = decl;
  auto raw = symbol.get();

  auto result = table->add(std::move(symbol));

  if (decl->isRoot) {
    if (!result.success) {
      switch (result.errorType) {
      case SymbolTable::Result::DUPLICATED:
        Error::diagnostic(decl->token,
                          "duplicated root variation name : " + decl->name);
        break;
      case SymbolTable::Result::RESERVED:
        Error::diagnostic(decl->token, "reserved name : " + decl->name);
        break;
      case SymbolTable::Result::UNKNOWN_SYMBOL:
        Error::internal(decl->token, "unknown symbol" + decl->name);
        break;
      case SymbolTable::Result::NONE:
        break;
      }
    }
  } else {
    if (!result.success) {
      switch (result.errorType) {
      case SymbolTable::Result::DUPLICATED:
        Error::diagnostic(decl->token,
                          "duplicated variation name : " + decl->name);
        break;
      case SymbolTable::Result::RESERVED:
        Error::diagnostic(decl->token, "reserved name : " + decl->name);
        break;
      case SymbolTable::Result::UNKNOWN_SYMBOL:
        Error::internal(decl->token, "unknown symbol" + decl->name);
        break;
      case SymbolTable::Result::NONE:
        break;
      }
    }
  }

  decl->symbol = raw;

  if (decl->init) {
    decl->init->accept(this);
  }
}

void Builder::visit(ArrayDecl *decl) {
  auto symbol = make_unique<ValueSymbol>();
  symbol->name = decl->name;
  symbol->kind = ValueSymbol::Kind::VAR;
  symbol->node = decl;
  auto raw = symbol.get();
  if (decl->isRoot) {
    auto result = table->add(std::move(symbol));

    if (!result.success) {
      switch (result.errorType) {
      case SymbolTable::Result::DUPLICATED:
        Error::diagnostic(decl->token,
                          "duplicated root variation name : " + decl->name);
        break;
      case SymbolTable::Result::RESERVED:
        Error::diagnostic(decl->token, "reserved name : " + decl->name);
        break;
      case SymbolTable::Result::UNKNOWN_SYMBOL:
        Error::internal(decl->token, "unknown symbol" + decl->name);
        break;
      case SymbolTable::Result::NONE:
        break;
      }
    }
  } else {
    auto result = table->add(std::move(symbol));

    if (!result.success) {
      switch (result.errorType) {
      case SymbolTable::Result::DUPLICATED:
        Error::diagnostic(decl->token,
                          "duplicated variation name : " + decl->name);
        break;
      case SymbolTable::Result::RESERVED:
        Error::diagnostic(decl->token, "reserved name : " + decl->name);
        break;
      case SymbolTable::Result::UNKNOWN_SYMBOL:
        Error::internal(decl->token, "unknown symbol" + decl->name);
        break;
      case SymbolTable::Result::NONE:
        break;
      }
    }
  }
  decl->symbol = raw;
  if (decl->init) {
    decl->init->accept(this);
  }
}

void Builder::visit(TypeNode *) {}
void Builder::visit(ASTNode *) {}
void Builder::visit(Param *a) {
  auto s = make_unique<ValueSymbol>();
  s->name = a->name;
  s->kind = ValueSymbol::Kind::PARAM;
  s->node = a;
  auto r = s.get();
  auto re = table->add(std::move(s));

  if (!re.success) {
    switch (re.errorType) {
    case SymbolTable::Result::DUPLICATED:
      Error::diagnostic(a->token, "duplicated class name : " + a->name);
      break;
    case SymbolTable::Result::RESERVED:
      Error::diagnostic(a->token, "reserved name : " + a->name);
      break;
    case SymbolTable::Result::UNKNOWN_SYMBOL:
      Error::internal(a->token, "unknown symbol" + a->name);
      break;
    case SymbolTable::Result::NONE:
      break;
    }
  }
  a->symbol = r;
}

void Builder::visit(InitDecl *decl) {
  auto symbol = make_unique<MethodSymbol>();

  symbol->name = decl->name;
  symbol->decl = decl;
  symbol->onwer = currentType;
  symbol->isInit = true;
  auto raw = symbol.get();

  auto result = table->addInit(std::move(symbol));

  if (!result.success) {
    switch (result.errorType) {
    case SymbolTable::Result::DUPLICATED:
      Error::diagnostic(decl->token, "duplicated method name : " + decl->name);
      break;
    case SymbolTable::Result::RESERVED:
      Error::diagnostic(decl->token, "reserved name : " + decl->name);
      break;
    case SymbolTable::Result::UNKNOWN_SYMBOL:
      Error::internal(decl->token, "unknown symbol" + decl->name);
      break;
    case SymbolTable::Result::NONE:
      break;
    }
  }
  decl->methodSymbol = raw;

  ScopeGuard _(*table);

  raw->scope = table->getCurrent();
  raw->isOverride = decl->isOverride;

  for (auto &a : decl->params) {
    auto s = make_unique<ValueSymbol>();
    s->name = a->name;
    s->kind = ValueSymbol::Kind::PARAM;
    s->node = a.get();
    auto r = s.get();
    auto re = table->add(std::move(s));

    if (!re.success) {
      switch (re.errorType) {
      case SymbolTable::Result::DUPLICATED:
        Error::diagnostic(a.get()->token,
                          "duplicated class name : " + a.get()->name);
        break;
      case SymbolTable::Result::RESERVED:
        Error::diagnostic(a.get()->token, "reserved name : " + a.get()->name);
        break;
      case SymbolTable::Result::UNKNOWN_SYMBOL:
        Error::internal(a.get()->token, "unknown symbol" + a.get()->name);
        break;
      case SymbolTable::Result::NONE:
        break;
      }
    }
    a->symbol = r;
    a->type->accept(this);
  }

  decl->body->accept(this);
}

bool Builder::canInnerDecl(Decl *decl) {
  switch (decl->kind) {

  case NKind::CLASS_DECL:
  case NKind::STRUCT_DECL:
  case NKind::ENUM_DECL:
    return true;
  default:
    return false;
  }
}