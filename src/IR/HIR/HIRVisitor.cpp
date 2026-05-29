#include "AST/Decl.h"
#include "AST/Expr.h"
#include "AST/Stmt.h"
#include "IR/HIR/HIRBuilder.h"
#include "IR/HIR/HIRExpr.h"
#include "IR/HIR/HIRHelper.h"
#include "IR/HIR/HIRProgram.h"
#include "IR/HIR/HIRStmt.h"
#include "IR/HIR/HIRSymbol.h"
#include "IR/HIR/HIRType.h"
#include "SemanticAnalyzer/SymbolTable.h"
#include "SemanticAnalyzer/symbol/Symbol.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "util/Error.h"
#include "util/Guard.h"
#include <memory>
#include <utility>

using std::unique_ptr;

void HIRBuilder::visit(LiteralExpr *expr) { exprResult = lowerLiteral(expr); }

void HIRBuilder::visit(BinaryExpr *expr) {
  auto left = lowerValue(expr->left.get());
  auto right = lowerValue(expr->right.get());
  auto type = HIRHelper::lowerType(program, source, expr->resolvedType);
  if (left == nullptr) {
    Error::internal(expr->left->span, "expr's left hir is nullptr");
  }
  if (right == nullptr) {
    Error::internal(expr->right->span, "expr's right hir is nullptr");
  }
  if (type == nullptr) {
    Error::internal(expr->span, "fail to get binary's type");
  }

  exprResult = make_unique<HIRBinaryExpr>(expr->span, type, expr->op,
                                          std::move(left), std::move(right));
}
void HIRBuilder::visit(NameExpr *expr) {
  if (expr == nullptr) {
    Error::internal("nameExpr is nullptr");
  }
  if (expr->resolved == nullptr) {
    Error::internal(expr->span, "nameExpr resolved is nullptr");
  }

  switch (expr->resolved->type) {
  case Symbol::SymbolType::VALUE: {
    auto place = lowerPlace(expr);
    if (place == nullptr) {
      Error::internal(expr->span, "failed to lower name expr as place");
    }
    exprResult = std::move(place);
    return;
  }

  case Symbol::SymbolType::TYPE:
    Error::internal(expr->span,
                    "type name cannot be used as standalone expression");
    return;

  default:
    Error::internal(expr->span, "unsupported resolved symbol in name expr");
  }
}

void HIRBuilder::visit(UnaryExpr *expr) {
  auto operand = lowerExpr(expr->right.get());
  exprResult = make_unique<HIRUnaryExpr>(
      expr->span, HIRHelper::lowerType(program, source, expr->resolvedType),
      expr->op, std::move(operand));
}

void HIRBuilder::visit(CallExpr *expr) {
  if (expr->receiver == nullptr) { // 해당 객체 내에서 this생략한 call
    if (expr->callType == CallExpr::CallType::INIT_CALL) {
      exprResult = lowerInitCall(expr);
      return;
    }
    exprResult = lowerImplictCall(expr);
    return;
  }
  if (isTypeReceiver(expr->receiver.get())) {
    auto type = dynamic_cast<TypeSymbol *>(
        dynamic_cast<NameExpr *>(expr->receiver.get())->resolved);

    if (type->kind == TypeSymbol::TypeKind::ENUM) {
      exprResult = lowerVariantValue(expr);
      return;
    } else {
      Error::internal("static method is not developed");
    }
  } else {
    if (expr->callType == CallExpr::CallType::INIT_CALL) {
      exprResult = lowerInitCall(expr);
      return;
    }
    exprResult = lowerCall(expr);
    return;
  }
}
void HIRBuilder::visit(AssignExpr *expr) { exprResult = lowerAssign(expr); }
void HIRBuilder::visit(MemberExpr *expr) {
  if (isTypeReceiver(expr->object.get())) {
    if (expr->object->resolvedType->kind == TypeSymbol::TypeKind::ENUM) {
      exprResult = lowerVariantValue(expr);
      return;
    } else {
      Error::internal("static field access is not developed");
    }
  } else {
    exprResult = lowerMember(expr);
    return;
  }
}
void HIRBuilder::visit(ArrayAccessExpr *expr) {
  exprResult = lowerArrayAccess(expr);
  return;
}
void HIRBuilder::visit(TernaryExpr *expr) {
  exprResult = lowerTernary(expr);
  return;
}
void HIRBuilder::visit(ThisExpr *) {
  exprResult = lowerImplictSelf();
  return;
}
void HIRBuilder::visit(SuperExpr *expr) {
  auto type = currentType->type;
  if (currentType->base == nullptr) {
    Error::internal(expr->span, "current Type has no parant type");
  }
  exprResult = make_unique<HIRSelfExpr>(expr->span, HIRSelfKind::This, type,
                                        type, currentType->base->type);
}
void HIRBuilder::visit(SelfExpr *) {
  exprResult = lowerImplictSelf();
  return;
}
void HIRBuilder::visit(RootExpr *expr) {
  exprResult = make_unique<HIRRootExpr>(expr->span, program->rootType);
  return;
}
void HIRBuilder::visit(CastExpr *expr) {
  exprResult = lowerCast(expr);
  return;
}
void HIRBuilder::visit(BuiltInNameExpr *) {}
void HIRBuilder::visit(SpawnExpr *expr) {
  exprResult = lowerSpawn(expr);
  return;
}
void HIRBuilder::visit(ViewExpr *expr) {
  exprResult = lowerView(expr);
  return;
}
void HIRBuilder::visit(DestroyExpr *expr) {
  Error::internal(expr->span, "not allowed destroy in expression");
}

void HIRBuilder::visit(QuitExpr *expr) {
  Error::internal(expr->span, "not allowed quit in expression");
}
void HIRBuilder::visit(DefaultValueExpr *) {}
void HIRBuilder::visit(Range *) {
  // for내부에서 처리
}
void HIRBuilder::visit(CaseValueExpr *) {
  // case value 내부에서 처리
}
void HIRBuilder::visit(MatchExpr *expr) { exprResult = lowerMatch(expr); }

// Statement HIRBuilder::visitor methods
void HIRBuilder::visit(ExprStmt *stmt) { emit(lowerExprStmt(stmt)); }
void HIRBuilder::visit(BlockStmt *stmt) {
  BoolGuard _(isField, false);
  emit(lowerBlock(stmt));
}
void HIRBuilder::visit(IfStmt *stmt) { emit(lowerIf(stmt)); }
void HIRBuilder::visit(ForStmt *stmt) { emit(lowerFor(stmt)); }
void HIRBuilder::visit(WhileStmt *stmt) { emit(lowerWhile(stmt)); }
void HIRBuilder::visit(SwitchStmt *stmt) { emit(lowerSwitch(stmt)); }
void HIRBuilder::visit(Case *) {}

void HIRBuilder::visit(ReturnStmt *stmt) { emit(lowerReturn(stmt)); }

void HIRBuilder::visit(ValueTransferStmt *stmt) {
  emit(lowerValueTransfer(stmt));
}
void HIRBuilder::visit(BreakStmt *stmt) {
  emit(make_unique<HIRBreakStmt>(stmt->span));
}
void HIRBuilder::visit(ContinueStmt *stmt) {
  emit(make_unique<HIRContinueStmt>(stmt->span));
}
void HIRBuilder::visit(DeclStmt *stmt) { stmt->decl->accept(this); }

void HIRBuilder::visit(EmptyStmt *) {}

// declare HIRBuilder::visitor methods
void HIRBuilder::visit(ClassDecl *decl) {
  auto it = program->typeDeclMap.find(decl->symbol);

  if (it == program->typeDeclMap.end()) {
    Error::internal(decl->span, "not made typeShell");
  }

  TypeGuard typeGuard(currentType, it->second);

  if (decl->baseClass.has_value()) {
    it = program->typeDeclMap.find(decl->symbol->base);
    if (it == program->typeDeclMap.end()) {
      Error::internal(decl->span, "not made typeShell");
    }
    currentType->base = it->second;
  }

  {
    BoolGuard fieldGuard(isField, true);
    for (auto &f : decl->fields) {
      f->accept(this);
    }
  }
  setDefaultInit(currentType);

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
    Error::internal(decl->span, "not made typeShell");
  }

  TypeGuard typeGuard(currentType, it->second);

  for (auto &f : decl->fields) {
    f->accept(this);
  }
  setDefaultInit(currentType);
  for (auto &i : decl->inits) {
    i->accept(this);
  }
}
void HIRBuilder::visit(EnumDecl *) {
  // linker 2-pass에서 처리
}
void HIRBuilder::visit(ImplDecl *decl) {
  auto typeSymbol = table->getType(decl->target);
  if (typeSymbol == nullptr) {
    Error::internal(decl->span, "fail to find impl target symbol");
  }

  auto it = program->typeDeclMap.find(typeSymbol);
  if (it == program->typeDeclMap.end()) {
    Error::internal(decl->span, "fail to find typeShell");
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
    auto it = program->rootMap.find(decl->symbol);
    if (it == program->rootMap.end()) {
      Error::internal(decl->span, "cannot find linked root : " + decl->name);
    }
  } else {
    if (isField) {
      auto it = currentType->fieldMap.find(decl->symbol);
      if (it == currentType->fieldMap.end()) {
        Error::internal(decl->span, "cannot find field");
      }
      auto field = it->second;
      if (field == nullptr) {
        Error::internal("field is nullptr");
      }
      if (decl->init) {
        currentType->defaultInit.emplace(field, decl->init.get());
      }
    } else {
      auto local = lowerLocal(decl);
      if (local == nullptr) {
        Error::internal("local is nullptr");
      }

      unique_ptr<HIRValueExpr> init = nullptr;
      if (decl->init) {
        init = lowerValue(decl->init.get());
        if (init == nullptr) {
          Error::internal("init is exist but nulltpr");
        }
      }
      emit(make_unique<HIRLocalDeclStmt>(decl->span, local, std::move(init)));
    }
  }
}
void HIRBuilder::visit(InitDecl *decl) { bindMethod(decl); }

void HIRBuilder::visit(TypeNode *) {}
void HIRBuilder::visit(ASTNode *node) {
  Error::internal(node->span, "unknown generic");
}
void HIRBuilder::visit(Param *) {}