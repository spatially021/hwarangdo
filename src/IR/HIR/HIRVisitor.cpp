#include "AST/Decl.h"
#include "AST/Expr.h"
#include "AST/Stmt.h"
#include "IR/HIR/HIRBuilder.h"
#include "IR/HIR/HIRExpr.h"
#include "IR/HIR/HIRStmt.h"
#include "IR/HIR/HIRSymbol.h"
#include "SemanticAnalyzer/SymbolTable.h"
#include "util/Error.h"
#include "util/Guard.h"
#include <memory>
#include <utility>

using std::unique_ptr;

void HIRBuilder::visit(LiteralExpr *expr) {
  if (!expr->resolvedType) {
    Error::internal(expr->token, "literal has no resolved type");
  }

  auto *ty = lowerType(expr->resolvedType);

  exprResult = std::make_unique<HIRLiteralExpr>(ty,
                                                expr->resolvedLit // 우선 copy
  );
}

void HIRBuilder::visit(BinaryExpr *expr) {
  auto left = lowerExpr(expr->left.get());
  auto right = lowerExpr(expr->right.get());
  exprResult =
      make_unique<HIRBinaryExpr>(lowerType(expr->resolvedType), expr->op,
                                 std::move(left), std::move(right));
}
void HIRBuilder::visit(NameExpr *expr) {
  if (expr->valueSymbol) {
    auto name = lowerPlace(expr);
    if (name == nullptr) {
      Error::internal("nameExpr is nullptr");
    }
    exprResult = std::move(name);
  } else if (expr->typeSymbol) {
  }
}
void HIRBuilder::visit(UnaryExpr *expr) {
  auto operand = lowerExpr(expr->right.get());
  exprResult = make_unique<HIRUnaryExpr>(lowerType(expr->resolvedType),
                                         expr->op, std::move(operand));
}
void HIRBuilder::visit(CallExpr *expr) {}
void HIRBuilder::visit(AssignExpr *expr) {}
void HIRBuilder::visit(MemberExpr *expr) {}
void HIRBuilder::visit(ArrayAccessExpr *expr) {}
void HIRBuilder::visit(TernaryExpr *expr) {}
void HIRBuilder::visit(ThisExpr *expr) {}
void HIRBuilder::visit(SuperExpr *expr) {}
void HIRBuilder::visit(CastExpr *expr) {}
void HIRBuilder::visit(BuiltInNameExpr *expr) {}
void HIRBuilder::visit(SpawnExpr *expr) {}
void HIRBuilder::visit(ViewExpr *expr) {}
void HIRBuilder::visit(DefaultValueExpr *expr) {}
void HIRBuilder::visit(Range *expr) {}
void HIRBuilder::visit(CaseValueExpr *expr) {}
void HIRBuilder::visit(MatchExpr *expr) {}

// Statement HIRBuilder::visitor methods
void HIRBuilder::visit(ExprStmt *stmt) {
  auto expr = lowerExpr(stmt->expr.get());
  emit(make_unique<HIRExprStmt>(std::move(expr)));
}
void HIRBuilder::visit(BlockStmt *stmt) {
  BoolGuard _(isField, false);
  emit(lowerBlock(stmt));
}
void HIRBuilder::visit(IfStmt *stmt) {
  unique_ptr<HIRExpr> cond = lowerExpr(stmt->condition.get());
  unique_ptr<HIRBlockStmt> thenBlock = lowerStmtAsBlock(stmt->thenBranch.get());
  unique_ptr<HIRBlockStmt> elseBlock =
      stmt->elseBranch ? lowerStmtAsBlock(stmt->elseBranch.get()) : nullptr;
  emit(make_unique<HIRIfStmt>(std::move(cond), std::move(thenBlock),
                              std::move(elseBlock)));
}
void HIRBuilder::visit(ForStmt *stmt) {
  HIRLocal *local = nullptr;

  if (auto d = dynamic_cast<DeclStmt *>(stmt->initializer.get())) {
    if (auto v = dynamic_cast<VarDecl *>(d->decl.get())) {
      local = lowerLocal(v);
    } else {
      Error::internal(stmt->token, "for initializer decl is not VarDecl");
    }
  } else {
    Error::internal(stmt->token, "for initializer is not DeclStmt");
  }

  auto from = lowerExpr(stmt->range->from.get());
  auto to = lowerExpr(stmt->range->to.get());
  auto step = lowerExpr(stmt->range->step.get());
  auto body = lowerStmtAsBlock(stmt->body.get());

  emit(make_unique<HIRForRangeStmt>(local, std::move(from), std::move(to),
                                    std::move(step), std::move(body)));
}
void HIRBuilder::visit(WhileStmt *stmt) {
  unique_ptr<HIRExpr> cond = lowerExpr(stmt->condition.get());
  unique_ptr<HIRBlockStmt> body = lowerStmtAsBlock(stmt->body.get());
  emit(make_unique<HIRWhileStmt>(std::move(cond), std::move(body)));
}
void HIRBuilder::visit(SwitchStmt *stmt) {
  unique_ptr<HIRExpr> value = lowerExpr(stmt->value.get());
  HIRLocal *temp = makeTemp(value->type);
  unique_ptr<HIRLocalPlaceExpr> tempPlace =
      make_unique<HIRLocalPlaceExpr>(temp);
}
void HIRBuilder::visit(Case *) {}

void HIRBuilder::visit(ReturnStmt *stmt) {
  std::unique_ptr<HIRExpr> expr = nullptr;
  if (stmt->value) {
    expr = lowerExpr(stmt->value.get());
  }
  emit(make_unique<HIRReturnStmt>(std::move(expr)));
}

void HIRBuilder::visit(ValueTransferStmt *stmt) {}
void HIRBuilder::visit(BreakStmt *) { emit(make_unique<HIRBreakStmt>()); }
void HIRBuilder::visit(ContinueStmt *) { emit(make_unique<HIRContinueStmt>()); }
void HIRBuilder::visit(DeclStmt *stmt) { stmt->decl->accept(this); }

void HIRBuilder::visit(EmptyStmt *) {}

// declare HIRBuilder::visitor methods
void HIRBuilder::visit(ClassDecl *decl) {
  auto it = program->typeDeclMap.find(decl->symbol);

  if (it == program->typeDeclMap.end()) {
    Error::internal(decl->token, "not made typeShell");
  }

  TypeGuard typeGuard(currentType, it->second);

  {
    BoolGuard fieldGuard(isField, true);
    for (auto &f : decl->fields) {
      f->accept(this);
    }
  }

  for (auto &m : decl->methods) {
    m->accept(this);
  }

  for (auto &i : decl->innerDecl) {
    i->accept(this);
  }
}

void HIRBuilder::visit(StructDecl *decl) {
  BoolGuard _(isField, true);
  auto it = program->typeDeclMap.find(decl->symbol);

  if (it == program->typeDeclMap.end()) {
    Error::internal(decl->token, "not made typeShell");
  }

  TypeGuard typeGuard(currentType, it->second);

  for (auto &f : decl->fields) {
    f->accept(this);
  }
}
void HIRBuilder::visit(EnumDecl *decl) {
  auto it = program->typeDeclMap.find(decl->symbol);

  if (it == program->typeDeclMap.end()) {
    Error::internal(decl->token, "not made typeShell");
  }

  TypeGuard typeGuard(currentType, it->second);

  for (auto &v : decl->variants) {
    auto variant = lowerEnumVariant(v.get());
    auto raw = variant.get();
    currentType->enumVariants.push_back(std::move(variant));
    auto [iter, result] = currentType->enumVariantMap.emplace(v->symbol, raw);
    if (!result) {
      Error::internal("fail to insert varaint");
    }
  }
}
void HIRBuilder::visit(ImplDecl *decl) {
  auto typeSymbol = table->getType(decl->target);
  if (typeSymbol == nullptr) {
    Error::internal(decl->token, "fail to find impl target symbol");
  }

  auto it = program->typeDeclMap.find(typeSymbol);
  if (it == program->typeDeclMap.end()) {
    Error::internal(decl->token, "fail to find typeShell");
  }

  TypeGuard typeGuard(currentType, it->second);
  for (auto &m : decl->LinkedImplMethods) {
    m->accept(this);
  }
}
void HIRBuilder::visit(TraitDecl *) {}
void HIRBuilder::visit(TraitSig *) {}
void HIRBuilder::visit(FuncDecl *decl) { bindMethod(decl); }
void HIRBuilder::visit(VarDecl *decl) {
  if (decl->isRoot) {
    // TODO:전역 구현
  } else {
    if (isField) {
      auto field = lowerField(decl);
      if (field == nullptr) {
        Error::internal("field is nullptr");
      }
      unique_ptr<HIRExpr> init = nullptr;
      if (decl->init) {
        init = lowerExpr(decl->init.get());
      }
    } else {
      auto local = lowerLocal(decl);
      if (local == nullptr) {
        Error::internal("local is nullptr");
      }

      unique_ptr<HIRExpr> init = nullptr;
      if (decl->init) {
        init = lowerExpr(decl->init.get());
        if (init == nullptr) {
          Error::internal("init is exist but nulltpr");
        }
      }
      emit(make_unique<HIRLocalDeclStmt>(local, std::move(init)));
    }
  }
}
void HIRBuilder::visit(ArrayDecl *decl) { // TODO:arrayDecl 설계 및 구현 필요.
}
void HIRBuilder::visit(InitDecl *decl) { bindMethod(decl); }

void HIRBuilder::visit(TypeNode *) {}
void HIRBuilder::visit(ASTNode *node) {
  Error::internal(node->token, "unknown generic");
}
void HIRBuilder::visit(Param *) {}