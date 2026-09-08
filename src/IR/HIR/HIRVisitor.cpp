#include "hrd/AST/Decl.h"
#include "hrd/AST/Expr.h"
#include "hrd/AST/Stmt.h"
#include "hrd/IR/HIR/HIRBuilder.h"
#include "hrd/IR/HIR/HIRExpr.h"
#include "hrd/IR/HIR/HIRProgram.h"
#include "hrd/IR/HIR/HIRStmt.h"
#include "hrd/IR/HIR/HIRSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/Symbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/diagnostic/Diagnostic.h"
#include "hrd/util/Error.h"
#include "hrd/util/Guard.h"
#include <memory>
#include <utility>
#include <variant>

using std::unique_ptr;

void HIRBuilder::visit(LiteralExpr *expr) { exprResult = lowerLiteral(expr); }

void HIRBuilder::visit(BinaryExpr *expr) {
  auto left = lowerValue(expr->left.get());
  auto right = lowerValue(expr->right.get());
  auto type = expr->resolvedType;

  if (left == nullptr) {
    Error::internal(expr->left->span, "binary left lowering returned nullptr");
  }

  if (right == nullptr) {
    Error::internal(expr->right->span,
                    "binary right lowering returned nullptr");
  }

  if (type == nullptr) {
    Error::internal(expr->span, "failed to lower binary result type");
  }

  exprResult =
      make_unique<HIRBinaryExpr>(expr->span, type, expr->op, std::move(left),
                                 std::move(right), expr->operrandType);
}

void HIRBuilder::visit(NameExpr *expr) {
  if (expr == nullptr) {
    Error::internal("NameExpr is nullptr");
  }

  if (expr->resolved == nullptr) {
    Error::internal(expr->span, "NameExpr resolved symbol is nullptr");
  }

  switch (expr->resolved->type) {
  case Symbol::SymbolType::VALUE: {
    auto place = lowerPlace(expr);

    if (place == nullptr) {
      Error::internal(expr->span, "failed to lower name expression as place");
    }

    exprResult = make_unique<HIRLoadExpr>(expr->span, std::move(place));
    return;
  }

  case Symbol::SymbolType::TYPE:
    Error::internal(expr->span,
                    "type name reached standalone expression lowering");

  default:
    Error::internal(expr->span,
                    "unsupported resolved symbol in name expression");
  }
}

void HIRBuilder::visit(UnaryExpr *expr) {
  auto operand = lowerExpr(expr->right.get());

  if (operand == nullptr) {
    Error::internal(expr->right->span,
                    "unary operand lowering returned nullptr");
  }

  auto *type = expr->resolvedType;
  if (type == nullptr) {
    Error::internal(expr->span, "failed to lower unary result type");
  }

  exprResult =
      make_unique<HIRUnaryExpr>(expr->span, type, expr->op, std::move(operand));
}

void HIRBuilder::visit(CallExpr *expr) {
  if (get_if<RuntimeSymbol *>(&expr->resolved)) {
    exprResult = lowerRuntime(expr);
    return;
  }

  if (expr->receiver == nullptr) {
    if (expr->callType == CallExpr::CallType::INIT_CALL) {
      exprResult = lowerInitCall(expr);
      return;
    }

    exprResult = lowerImplictCall(expr);
    return;
  }

  if (isTypeReceiver(expr->receiver.get())) {
    auto *name = dynamic_cast<NameExpr *>(expr->receiver.get());
    if (name == nullptr) {
      Error::internal(expr->receiver->span,
                      "type receiver is not a name expression");
    }

    auto *type = dynamic_cast<TypeSymbol *>(name->resolved);
    if (type == nullptr) {
      Error::internal(expr->receiver->span,
                      "type receiver did not resolve to TypeSymbol");
    }

    if (type->kind == TypeKind::ENUM) {
      exprResult = lowerVariantValue(expr);
      return;
    }

    if (isa<ObjectType>(type)) {
      exprResult = lowerCall(expr);
      return;
    }
  }

  if (expr->callType == CallExpr::CallType::INIT_CALL) {
    exprResult = lowerInitCall(expr);
    return;
  }

  exprResult = lowerCall(expr);
}

void HIRBuilder::visit(AssignExpr *expr) {
  auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_H008);
  dia.labels = {
      {expr->span, "this assignment is used as a value expression", true},
  };
  dia.notes = {
      "assignment does not produce a value",
  };
  dia.helps = {
      "move the assignment into its own statement",
  };
  engine.emit(dia);
  recover.recover();
}

void HIRBuilder::visit(MemberExpr *expr) {
  if (isTypeReceiver(expr->object.get())) {
    if (expr->object->resolvedType == nullptr) {
      Error::internal(expr->object->span,
                      "member type receiver has no resolved type");
    }

    if (expr->object->resolvedType->kind == TypeKind::ENUM) {
      exprResult = lowerVariantValue(expr);
      return;
    }

    Error::internal(expr->span, "static field access reached HIR lowering");
  }

  exprResult = lowerMember(expr);
}

void HIRBuilder::visit(ArrayAccessExpr *expr) {
  exprResult = lowerArrayAccess(expr);
}

void HIRBuilder::visit(ArrayLiteralExpr *expr) {
  exprResult = lowerArrayLiteral(expr);
}

void HIRBuilder::visit(TernaryExpr *expr) { exprResult = lowerTernary(expr); }

void HIRBuilder::visit(ThisExpr *) { exprResult = lowerImplictSelf(); }

void HIRBuilder::visit(SuperExpr *expr) {
  if (currentType == nullptr) {
    Error::internal(expr->span, "current HIR type is nullptr");
  }

  if (currentType->type == nullptr) {
    Error::internal(expr->span, "current HIR type representation is nullptr");
  }

  if (currentType->base == nullptr) {
    Error::internal(expr->span,
                    "super expression reached a type without a base type");
  }

  if (currentType->base->type == nullptr) {
    Error::internal(expr->span, "base HIR type representation is nullptr");
  }

  auto *type = currentType->type;

  exprResult = make_unique<HIRSelfExpr>(expr->span, HIRSelfKind::This, type,
                                        type, currentType->base->type);
}

void HIRBuilder::visit(SelfExpr *) { exprResult = lowerImplictSelf(); }

void HIRBuilder::visit(RootExpr *expr) {
  exprResult = make_unique<HIRRootExpr>(expr->span, expr->resolvedType);
}

void HIRBuilder::visit(CastExpr *expr) { exprResult = lowerCast(expr); }

void HIRBuilder::visit(BuiltInNameExpr *) {}

void HIRBuilder::visit(SpawnExpr *expr) { exprResult = lowerSpawn(expr); }

void HIRBuilder::visit(ViewExpr *expr) { exprResult = lowerView(expr); }

void HIRBuilder::visit(DestroyExpr *expr) {
  auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_H009);
  dia.labels = {
      {expr->span, "this destroy operation is used as a value expression",
       true},
  };
  dia.notes = {
      "'world.destroy' removes an entity and does not produce a value",
  };
  dia.helps = {
      "move this destroy operation into its own statement",
  };
  engine.emit(dia);
  recover.recover();
}

void HIRBuilder::visit(QuitExpr *expr) {
  auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_H010);
  dia.labels = {
      {expr->span, "this quit operation is used as a value expression", true},
  };
  dia.notes = {
      "quit terminates execution and does not produce a value",
  };
  dia.helps = {
      "use quit as a standalone statement",
  };
  engine.emit(dia);
  recover.recover();
}

void HIRBuilder::visit(DefaultValueExpr *expr) {
  Error::internal(expr->span,
                  "default value expression remained after argument lowering");
}

void HIRBuilder::visit(Range *) {
  // Lowered as part of ForStmt.
}

void HIRBuilder::visit(CaseValueExpr *) {
  // Lowered as part of Case.
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

void HIRBuilder::visit(Case *) {
  // Case is lowered through switch/match lowering.
}

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

// Declaration HIRBuilder::visitor methods
void HIRBuilder::visit(ClassDecl *decl) {
  auto it = program->typeDeclMap.find(decl->symbol);

  if (it == program->typeDeclMap.end() || it->second == nullptr) {
    Error::internal(decl->span, "class HIR type shell was not created");
  }

  TypeGuard typeGuard(currentType, it->second);

  if (decl->baseClass.has_value()) {
    auto obj = dyn_cast<ObjectType>(decl->symbol);
    if (obj->base == nullptr) {
      Error::internal(decl->span, "class base symbol is nullptr");
    }

    it = program->typeDeclMap.find(obj->base);

    if (it == program->typeDeclMap.end() || it->second == nullptr) {
      Error::internal(decl->span, "base class HIR type shell was not created");
    }

    currentType->base = it->second;
  }

  {
    BoolGuard fieldGuard(isField, true);

    for (auto &field : decl->fields) {
      field->accept(this);
    }
  }

  setDefaultInit(currentType);

  for (auto &method : decl->methods) {
    method->accept(this);
  }
}

void HIRBuilder::visit(StructDecl *decl) {
  auto it = program->typeDeclMap.find(decl->symbol);

  if (it == program->typeDeclMap.end() || it->second == nullptr) {
    Error::internal(decl->span, "struct HIR type shell was not created");
  }

  TypeGuard typeGuard(currentType, it->second);

  {
    BoolGuard fieldGuard(isField, true);

    for (auto &field : decl->fields) {
      field->accept(this);
    }
  }

  setDefaultInit(currentType);

  for (auto &init : decl->inits) {
    init->accept(this);
  }
}

void HIRBuilder::visit(EnumDecl *) {
  // Lowered during linker two-pass processing.
}

void HIRBuilder::visit(ImplDecl *decl) {
  auto *typeSymbol = decl->importTarget;

  if (typeSymbol == nullptr) {
    Error::internal(decl->span, "failed to find impl target symbol");
  }

  auto it = program->typeDeclMap.find(typeSymbol);

  if (it == program->typeDeclMap.end() || it->second == nullptr) {
    Error::internal(decl->span, "impl target HIR type shell was not created");
  }

  TypeGuard typeGuard(currentType, it->second);

  for (auto &method : decl->LinkedImplMethods) {
    method->accept(this);
  }
}

void HIRBuilder::visit(TraitDecl *) {}

void HIRBuilder::visit(TraitSig *) {}

void HIRBuilder::visit(FuncDecl *decl) { bindMethod(decl); }

void HIRBuilder::visit(VarDecl *decl) {
  if (decl->isRoot) {
    return;
  }

  if (isField) {
    if (currentType == nullptr) {
      Error::internal(decl->span, "field declaration has no current HIR type");
    }

    auto *field = decl->symbol;

    if (decl->init) {
      currentType->defaultInit.emplace(field, decl->init.get());
    }

    return;
  }

  auto *local = lowerLocal(decl);

  if (local == nullptr) {
    Error::internal(decl->span, "local lowering returned nullptr");
  }

  if (local->type == nullptr) {
    Error::internal(decl->span, "local HIR type is nullptr");
  }

  unique_ptr<HIRValueExpr> init = nullptr;

  if (decl->init) {
    init = lowerValue(decl->init.get());

    if (init == nullptr) {
      Error::internal(decl->init->span,
                      "local initializer lowering returned nullptr");
    }
  }

  emit(make_unique<HIRLocalDeclStmt>(decl->span, local, std::move(init)));
}

void HIRBuilder::visit(InitDecl *decl) { bindMethod(decl); }

void HIRBuilder::visit(OnDestroyDecl *decl) { bindMethod(decl); }

void HIRBuilder::visit(TypeNode *) {}

void HIRBuilder::visit(ASTNode *node) {
  Error::internal(node->span, "unsupported generic AST node");
}

void HIRBuilder::visit(Param *) {}
void HIRBuilder::visit(ImportDecl *) {}