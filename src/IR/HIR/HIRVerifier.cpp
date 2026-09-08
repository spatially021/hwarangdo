#include "hrd/IR/HIR/HIRVerifier.h"
#include "hrd/AST/Stmt.h"
#include "hrd/IR/HIR/HIRDecl.h"
#include "hrd/IR/HIR/HIRExpr.h"
#include "hrd/IR/HIR/HIRNode.h"
#include "hrd/IR/HIR/HIRPattern.h"
#include "hrd/IR/HIR/HIRProgram.h"
#include "hrd/IR/HIR/HIRStmt.h"
#include "hrd/IR/HIR/HIRSymbol.h"
#include "hrd/Recover/HIRVerifierRecover.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/compiler/CompilerContexts.h"
#include "hrd/diagnostic/Diagnostic.h"
#include "hrd/enums/HirExpect.h"
#include "hrd/enums/InheritState.h"
#include "hrd/util/Error.h"
#include "magic_enum/magic_enum.hpp"
#include <cassert>
#include <llvm/ADT/APInt.h>
#include <llvm/ADT/SmallString.h>
#include <string>
#include <vector>

HIRVerifier::HIRVerifier(HIRVerifierContext &ctx)
    : program(ctx.program), engine(ctx.engine), recover(*this) {}

void HIRVerifier::verify() {
  if (program == nullptr) {
    Error::internal("program is nullptr");
  }

  for (auto &t : program->typeDeclMap) {
    if (inheritStates[t.second] == InheritState::Unvisited) {
      verifyBlock(t.second->defaultInitBlock.get());
      verifyType(t.second);
    }
  }
}

void HIRVerifier::verifyType(HIRTypeDecl *type) {
  if (inheritStates[type] == InheritState::Done) {
    return;
  }
  inheritStates[type] = InheritState::Visiting;
  if (type->base) {
    if (inheritStates[type->base] == InheritState::Visiting) {
      Error::internal(type->span, "cyclic inhernit");
    } else if (inheritStates[type->base] == InheritState::Unvisited) {
      verifyType(type->base);
    }
  }

  if (type == nullptr) {
    Error::internal("hirTypeDecl is nullptr");
  }

  if (type->type == nullptr) {
    Error::internal(type->span, "hirType is nullptr");
  }

  if (type == nullptr) {
    Error::internal("hirTypeDecl is nullptr");
  }

  if (type->type == nullptr) {
    Error::internal(type->span, "hirType is nullptr");
  }

  if (type->typeDeclKind == HIRTypeDeclKind::Enum) {
    return;
  }

  for (auto &i : type->initMap) {
    verifyMethod(i.second, type);
  }

  for (auto &m : type->methodMap) {
    verifyMethod(m.second, type);
  }

  inheritStates[type] = InheritState::Done;
}

void HIRVerifier::verifyMethod(HIRMethodDecl *method, HIRTypeDecl *) {
  if (method == nullptr) {
    Error::internal("hirMethodDecl is nullptr");
  }

  for (auto &p : method->paramMap) {
    verifyParam(p.second);
  }

  if (!method->isExtern) {
    if (method->body == nullptr) {
      Error::internal("method body is nullptr");
    }

    verifyBlock(method->body.get());

    if (method->returnType->kind != TypeKind::VOID &&
        !definitelyReturns(method->body.get())) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_H001);
      dia.labels = {
          {method->span,
           "method '" + method->name +
               "' may reach the end without returning a value",
           true},
      };
      dia.notes = {
          "the declared return type is '" + method->returnType->name + "'",
          "every reachable control-flow path must return a value",
      };
      dia.helps = {
          "add a return statement to every reachable path",
      };
      engine.emit(dia);
      recover.recover();
    }
  }

  if (method->returnType == nullptr) {
    Error::internal(method->name + "'s return type is nullptr");
  }
}

void HIRVerifier::verifyParam(HIRParam *param) {
  if (param == nullptr) {
    Error::internal("param is nullptr");
  }

  if (param->type == nullptr) {
    Error::internal("param's type is nullptr");
  }
}

void HIRVerifier::verifyBlock(HIRBlockStmt *stmt) {
  if (stmt == nullptr) {
    Error::internal("block stmt is nullptr");
  }

  for (auto &l : stmt->localMap) {
    verifyLocal(l.second);
  }

  for (auto &s : stmt->statements) {
    if (s == nullptr) {
      Error::internal("stmt is nullptr");
    }

    verifyStmt(s.get());
  }
}

void HIRVerifier::verifyLocal(HIRLocal *local) {
  if (local == nullptr) {
    Error::internal("local is nullptr");
  }
  if (local->type == nullptr) {
    Error::internal("local's type is nullptr");
  }
  if (local->type->kind == TypeKind::CLASS) {
    if (!local->isInitialized) {
      Error::internal("local is observer but not initialized");
    }
  }
}

void HIRVerifier::verifyStmt(HIRStmt *stmt) {
  if (stmt == nullptr) {
    Error::internal("stmt is nullptr");
  }

  switch (stmt->kind) {

  case HIRNodeKind::BlockStmt: {
    auto *block = expect<HIRBlockStmt>(stmt, HIRNodeKind::BlockStmt);
    verifyBlock(block);
    break;
  }
  case HIRNodeKind::ExprStmt: {
    auto exprStmt = expect<HIRExprStmt>(stmt, HIRNodeKind::ExprStmt);
    if (exprStmt->expr == nullptr) {
      Error::internal("exprStmt's expr is nullptr");
    }
    verifyExpr(exprStmt->expr.get());
    break;
  }
  case HIRNodeKind::IfStmt: {
    auto ifStmt = expect<HIRIfStmt>(stmt, HIRNodeKind::IfStmt);

    if (ifStmt->condition == nullptr) {
      Error::internal("ifStmt's condition is nullptr");
    }
    if (ifStmt->thenBlock == nullptr) {
      Error::internal("ifStmt's thenBlock is nullptr");
    }

    if (ifStmt->condition->type == nullptr) {
      Error::internal("ifStmt's condition type is nullptr");
    }
    if (!isa<BoolType>(ifStmt->condition->type)) {
      Error::internal("ifStmt's condition is not bool type : " +
                      ifStmt->condition->type->name);
    }

    verifyExpr(ifStmt->condition.get());

    verifyBlock(ifStmt->thenBlock.get());

    if (ifStmt->elseBlock != nullptr) {
      verifyBlock(ifStmt->elseBlock.get());
    }

    break;
  }
  case HIRNodeKind::WhileStmt: {
    auto whileStmt = expect<HIRWhileStmt>(stmt, HIRNodeKind::WhileStmt);

    if (whileStmt->condition == nullptr) {
      Error::internal("whileStmt's condition is nullptr");
    }
    if (whileStmt->body == nullptr) {
      Error::internal("whileStmt's body is nullptr");
    }
    verifyExpr(whileStmt->condition.get());
    if (whileStmt->condition->type == nullptr) {
      Error::internal("whileStmt's condition type is nullptr");
    }
    if (isa<BoolType>(whileStmt->condition->type)) {
      Error::internal("whileStmt's condition is not bool type");
    }

    verifyBlock(whileStmt->body.get());
    break;
  }
  case HIRNodeKind::ForRangeStmt: {
    auto range = expect<HIRForRangeStmt>(stmt, HIRNodeKind::ForRangeStmt);

    if (range->start == nullptr) {
      Error::internal("forRangeStmt's start is nullptr");
    }
    if (range->end == nullptr) {
      Error::internal("forRangeStmt's end is nullptr");
    }
    if (range->step == nullptr) {
      Error::internal("forRangeStmt's step is nullptr");
    }
    if (range->indexVar == nullptr) {
      Error::internal("forRangeStmt's indexVar is nullptr");
    }
    if (range->body == nullptr) {
      Error::internal("forRangeStmt's body is nullptr");
    }

    verifyExpr(range->start.get());
    verifyExpr(range->end.get());
    verifyExpr(range->step.get());
    verifyLocal(range->indexVar);
    verifyBlock(range->body.get());
    break;
  }
  case HIRNodeKind::ReturnStmt: {
    auto returnStmt = expect<HIRReturnStmt>(stmt, HIRNodeKind::ReturnStmt);
    if (returnStmt->value != nullptr) {
      if (returnStmt->value->type == nullptr) {
        Error::internal("returnStmt's return value's type is nullptr");
      }
      verifyExpr(returnStmt->value.get());
    }
    break;
  }
  case HIRNodeKind::BreakStmt: {
    auto br = expect<HIRBreakStmt>(stmt, HIRNodeKind::BreakStmt);
    (void)br;
    break;
  }
  case HIRNodeKind::ContinueStmt: {
    auto con = expect<HIRContinueStmt>(stmt, HIRNodeKind::ContinueStmt);
    (void)con;
    break;
  }
  case HIRNodeKind::SwitchStmt: {
    auto switchStmt = expect<HIRSwitchStmt>(stmt, HIRNodeKind::SwitchStmt);

    if (switchStmt->cond == nullptr) {
      Error::internal("switchStmt's cond is nullptr");
    }

    verifyExpr(switchStmt->cond.get());
    for (auto &a : switchStmt->cases) {
      if (a == nullptr) {
        Error::internal("switchStmt's case is nullptr");
      }
      verifyStmt(a.get());
    }
    break;
  }
  case HIRNodeKind::Case: {
    auto caseStmt = expect<HIRCase>(stmt, HIRNodeKind::Case);
    if (caseStmt->defaultKind == HIRDefaultKind::Default) {
      if (!caseStmt->selectors.empty()) {
        Error::internal("default has selector");
      }
    }
    for (auto &s : caseStmt->selectors) {
      if (s == nullptr) {
        Error::internal("caseStmt's selector is nullptr");
      }
      verifyCasePattern(s.get());
    }

    if (caseStmt->body == nullptr) {
      Error::internal("caseStmt's body is nullptr");
    }
    verifyBlock(caseStmt->body.get());
    break;
  }
  case HIRNodeKind::OnExitStmt: {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_H002);
    dia.labels = {
        {stmt->span, "onexit cannot be verified in the current HIR", true},
    };
    dia.notes = {
        "onexit lowering and verification are not implemented yet",
    };
    dia.helps = {
        "remove this onexit statement until the feature is implemented",
    };
    engine.emit(dia);
    recover.recover();
    break;
  }
  case HIRNodeKind::ValueTransferStmt: {
    auto value =
        expect<HIRValueTransferStmt>(stmt, HIRNodeKind::ValueTransferStmt);

    if (value->value == nullptr) {
      Error::internal("valueTransferStmt's value is nullptr");
    }
    verifyExpr(value->value.get());
    break;
  }

  case HIRNodeKind::DestroyStmt: {
    auto destroy = expect<HIRDestroyStmt>(stmt, HIRNodeKind::DestroyStmt);
    if (destroy->entity == nullptr) {
      Error::internal("destroyStmt's entity is nullptr");
    }
    if (destroy->handle == nullptr) {
      Error::internal("destroyStmt's handle is nullptr");
    }
    verifyExpr(destroy->handle.get());
    break;
  }

  case HIRNodeKind::LocalDeclStmt: {
    auto local = expect<HIRLocalDeclStmt>(stmt, HIRNodeKind::LocalDeclStmt);
    verifyLocal(local->local);
    if (local->init != nullptr) {
      verifyExpr(local->init.get());
    }
    break;
  }

  case HIRNodeKind::QuitStmt: {
    break;
  }
  case HIRNodeKind::AssignStmt: {
    auto assign = expect<HIRAssignStmt>(stmt, HIRNodeKind::AssignStmt);
    if (assign->lhs == nullptr) {
      Error::internal(assign->span, "assign's lhs is nullptr");
    }
    if (assign->lhs->type->kind == TypeKind::CLASS) {
      Error::internal(assign->span, "observer cannot be assigned");
    }
    if (assign->rhs == nullptr) {
      Error::internal(assign->span, "assign's rhs is nullptr");
    }
    verifyExpr(assign->lhs.get());
    verifyExpr(assign->rhs.get());
    break;
  }
  case HIRNodeKind::CompoundAssignStmt: {
    auto compound =
        expect<HIRCompoundAssignStmt>(stmt, HIRNodeKind::CompoundAssignStmt);
    if (compound->lhs == nullptr) {
      Error::internal("compoundAssignExpr's lhs is nullptr");
    }
    if (compound->lhs->type->kind == TypeKind::CLASS) {
      Error::internal("observer cannot be assigned");
    }
    if (compound->rhs == nullptr) {
      Error::internal("compoundAssignExpr's rhs is nullptr");
    }
    verifyExpr(compound->lhs.get());
    verifyExpr(compound->rhs.get());
    break;
  }
  default:
    Error::internal("illegal stmt kind : " +
                    std::string(magic_enum::enum_name(stmt->kind)));
    break;
  }
}

void HIRVerifier::verifyExpr(HIRExpr *expr, bool) {
  if (expr == nullptr) {
    Error::internal("expr is nullptr");
  }

  switch (expr->kind) {

  case HIRNodeKind::LiteralExpr: {
    auto literal = expect<HIRLiteralExpr>(expr, HIRNodeKind::LiteralExpr);
    if (literal->type == nullptr) {
      Error::internal("literal's type is nullptr");
    }
    break;
  }
  case HIRNodeKind::ArrayLiteralExpr: {
    auto literal =
        expect<HIRArrayLiteralExpr>(expr, HIRNodeKind::ArrayLiteralExpr);
    if (literal->type == nullptr) {
      Error::internal("literal's type is nullptr");
    }
    for (auto &e : literal->elements) {
      verifyExpr(e.get());
    }
    break;
  }
  case HIRNodeKind::LoadExpr: {
    auto load = expect<HIRLoadExpr>(expr, HIRNodeKind::LoadExpr);
    if (load->type == nullptr) {
      Error::internal("loadExpr's type is nullptr");
    }
    if (load->place == nullptr) {
      Error::internal("loadExpr's place is nullptr");
    }
    verifyExpr(load->place.get());
    break;
  }
  case HIRNodeKind::UnaryExpr: {
    auto unary = expect<HIRUnaryExpr>(expr, HIRNodeKind::UnaryExpr);
    if (unary->type == nullptr) {
      Error::internal("unary's type is nullptr");
    }
    if (unary->operand == nullptr) {
      Error::internal("unary's operand is nullptr");
    }
    verifyExpr(unary->operand.get());
    break;
  }
  case HIRNodeKind::BinaryExpr: {
    auto binary = expect<HIRBinaryExpr>(expr, HIRNodeKind::BinaryExpr);
    if (binary->type == nullptr) {
      Error::internal("binary's type is nullptr");
    }
    if (binary->left == nullptr) {
      Error::internal("binary's left is nullptr");
    }
    if (binary->right == nullptr) {
      Error::internal("binary's right is nullptr");
    }
    verifyExpr(binary->left.get());
    verifyExpr(binary->right.get());
    break;
  }

  case HIRNodeKind::CastExpr: {
    auto cast = expect<HIRCastExpr>(expr, HIRNodeKind::CastExpr);
    if (cast->type == nullptr) {
      Error::internal("cast's type is nullptr");
    }
    if (cast->fromType == nullptr) {
      Error::internal("cast's fromType is nullptr");
    }
    if (cast->toType == nullptr) {
      Error::internal("cast's toType is nullptr");
    }
    if (cast->operand == nullptr) {
      Error::internal("cast's operand is nullptr");
    }
    verifyExpr(cast->operand.get());
    break;
  }
  case HIRNodeKind::MethodCallExpr: {
    auto call = expect<HIRMethodCallExpr>(expr, HIRNodeKind::MethodCallExpr);
    if (call->type == nullptr) {
      Error::internal("methodCall's type is nullptr");
    }
    if (!call->method->isStatic) {
      if (call->receiver == nullptr) {
        Error::internal("methodCall's receiver is nullptr");
      }
      verifyExpr(call->receiver.get());
    }

    if (call->method->params.size() != call->args.size()) {
      Error::internal("call argument count mismatch");
    }

    for (auto &a : call->args) {
      if (a == nullptr) {
        Error::internal("methodCall's arg is nullptr");
      }
      if (a->kind == HIRNodeKind::DefaultValueExpr) {
        Error::internal("methodCall's defefaultValue is remained");
      }
      if (a->type == nullptr) {
        Error::internal("methodCall's arg's type is nullptr");
      }
      verifyExpr(a.get());
    }
    break;
  }
  case HIRNodeKind::SpawnExpr: {
    auto spawn = expect<HIRSpawnExpr>(expr, HIRNodeKind::SpawnExpr);

    if (spawn->type == nullptr) {
      Error::internal("spawnExpr's type is nullptr");
    }

    if (spawn->entityType == nullptr) {
      Error::internal("spawnExpr's entityType is nullptr");
    }

    for (auto &a : spawn->args) {
      if (a == nullptr) {
        Error::internal("spawnExpr's arg is nullptr");
      }
      verifyExpr(a.get());
    }
    break;
  }
  case HIRNodeKind::ViewExpr: {
    auto view = expect<HIRViewExpr>(expr, HIRNodeKind::ViewExpr);
    if (view->type == nullptr) {
      Error::internal("viewExpr's type is nullptr");
    }
    auto ob = view->type;
    if (ob == nullptr) {
      Error::internal("viewExpr's resultType is not oberver type");
    }
    if (view->entityType == nullptr) {
      Error::internal("viewExpr's entityType is nullptr");
    }
    if (view->handle == nullptr) {
      Error::internal("viewExpr's handle is nullptr");
    }
    verifyExpr(view->handle.get());
    break;
  }
  case HIRNodeKind::MatchExpr: {
    auto match = expect<HIRMatchExpr>(expr, HIRNodeKind::MatchExpr);
    if (match->type == nullptr) {
      Error::internal("match's type is nullptr");
    }
    if (match->cond == nullptr) {
      Error::internal("match's cond is nullptr");
    }
    verifyExpr(match->cond.get());
    for (auto &c : match->cases) {
      if (c == nullptr) {
        Error::internal("match's case is nullptr");
      }
      verifyStmt(c.get());
    }
    break;
  }
  case HIRNodeKind::TernaryExpr: {
    auto ternary = expect<HIRTernaryExpr>(expr, HIRNodeKind::TernaryExpr);
    if (ternary->type == nullptr) {
      Error::internal("ternary's type is nullptr");
    }
    if (ternary->condition == nullptr) {
      Error::internal("ternary's condition is nullptr");
    }
    if (ternary->thenExpr == nullptr) {
      Error::internal("ternary's thenExpr is nullptr");
    }
    if (ternary->elseExpr == nullptr) {
      Error::internal("ternary's elseExpr is nullptr");
    }
    verifyExpr(ternary->condition.get());
    if (ternary->condition->type == nullptr) {
      Error::internal("ternary's condition type is nullptr");
    }
    if (!isa<BoolType>(ternary->condition->type)) {
      Error::internal("ternary's condition is not bool type");
    }
    verifyExpr(ternary->thenExpr.get());
    verifyExpr(ternary->elseExpr.get());
    break;
  }
  case HIRNodeKind::DefaultValueExpr: {
    Error::internal("defaultValueExpr remained after HIR lowering");
    break;
  }
  case HIRNodeKind::LocalPlaceExpr: {
    auto local = expect<HIRLocalPlaceExpr>(expr, HIRNodeKind::LocalPlaceExpr);
    if (local->type == nullptr) {
      Error::internal("localPlaceExpr's type is nullptr");
    }
    if (local->local == nullptr) {
      Error::internal("localPlaceExpr's local is nullptr");
    }
    verifyLocal(local->local);
    break;
  }
  case HIRNodeKind::ParamPlaceExpr: {
    auto param = expect<HIRParamPlaceExpr>(expr, HIRNodeKind::ParamPlaceExpr);
    if (param->type == nullptr) {
      Error::internal("paramPlaceExpr's type is nullptr");
    }
    if (param->param == nullptr) {
      Error::internal("paramPlaceExpr's param is nullptr");
    }
    verifyParam(param->param);
    break;
  }
  case HIRNodeKind::FieldPlaceExpr: {
    auto field = expect<HIRFieldPlaceExpr>(expr, HIRNodeKind::FieldPlaceExpr);
    if (field->type == nullptr) {
      Error::internal("fieldPlaceExpr's type is nullptr");
    }
    if (field->receiver == nullptr) {
      Error::internal("fieldPlaceExpr's receiver is nullptr");
    }
    if (field->field == nullptr) {
      Error::internal("fieldPlaceExpr's field is nullptr");
    }
    verifyExpr(field->receiver.get());

    break;
  }
  case HIRNodeKind::SelfExpr: {
    auto self = expect<HIRSelfExpr>(expr, HIRNodeKind::SelfExpr);
    if (self->type == nullptr) {
      Error::internal("self's type is nullptr");
    }
    if (self->accessType == nullptr) {
      Error::internal("self's accessType is nullptr");
    }
    if (self->ownerType == nullptr) {
      Error::internal("self's ownerType is nullptr");
    }
    break;
  }
  case HIRNodeKind::RootExpr: {
    auto root = expect<HIRRootExpr>(expr, HIRNodeKind::RootExpr);
    if (root->type == nullptr) {
      Error::internal("root's type is nullptr");
    }
    break;
  }
  case HIRNodeKind::ArrayAccessExpr: {
    auto arr =
        expect<HIRArrayAccessPlaceExpr>(expr, HIRNodeKind::ArrayAccessExpr);
    if (arr->type == nullptr) {
      Error::internal("arrayAccessPlaceExpr's type is nullptr");
    }
    if (arr->elementType == nullptr) {
      Error::internal("arrayAccessPlaceExpr's elementType is nullptr");
    }
    if (arr->object == nullptr) {
      Error::internal("arrayAccessPlaceExpr's object is nullptr");
    }
    if (arr->index == nullptr) {
      Error::internal("arrayAccessPlaceExpr's index is nullptr");
    }
    verifyExpr(arr->object.get(), false);
    verifyExpr(arr->index.get());
    break;
  }
  case HIRNodeKind::EnumVariantValue: {
    auto varaint =
        expect<HIRVariantValueExpr>(expr, HIRNodeKind::EnumVariantValue);
    if (varaint->type == nullptr) {
      Error::internal("variantValueExpr's type is nullptr");
    }
    if (varaint->varaint == nullptr) {
      Error::internal("variantValueExpr's variant is nullptr");
    }
    if (varaint->varaint->payloadType != nullptr) {
      if (varaint->payload == nullptr) {
        Error::internal("variantValueExpr's payload is nullptr");
      }
      verifyExpr(varaint->payload.get());
    }
    break;
  }
  case HIRNodeKind::StructInitExpr: {
    auto init = expect<HIRStructInitExpr>(expr, HIRNodeKind::StructInitExpr);
    if (init->type == nullptr) {
      Error::internal(init->span, "struct init's type is nullptr");
    }

    if (init->isDefault) {
      break;
    }

    for (auto &a : init->args) {
      if (a == nullptr) {
        Error::internal("methodCall's arg is nullptr");
      }
      if (a->kind == HIRNodeKind::DefaultValueExpr) {
        Error::internal("methodCall's defefaultValue is remained");
      }
      if (a->type == nullptr) {
        Error::internal("methodCall's arg's type is nullptr");
      }
      verifyExpr(a.get());
    }
    break;
  }

  case HIRNodeKind::RuntimeCallExpr: {
    auto runtime = expect<HIRRuntimeCall>(expr, HIRNodeKind::RuntimeCallExpr);
    if (runtime->symbol == nullptr) {
      Error::internal(expr->span, "runtime's symbol is nullptr");
    }
    for (auto &a : runtime->args) {
      verifyExpr(a.get());
    }
    break;
  };

  default:
    Error::internal("illegal expr kind");
    break;
  }
}

void HIRVerifier::verifyCasePattern(HIRCasePattern *pattern) {
  std::visit(
      [&](auto &selector) {
        using T = std::decay_t<decltype(selector)>;

        if constexpr (std::is_same_v<T, HIRLiteralCase>) {
          // selector.expr 사용
          verifyExpr(selector.expr.get());
        } else if constexpr (std::is_same_v<T, HIRUnitCase>) {
          // selector.variant 사용
          if (selector.variant == nullptr) {
            Error::internal("variant is nullptr");
          }
        } else if constexpr (std::is_same_v<T, HIRPayloadCase>) {
          // selector.variant, selector.binding 사용
          if (selector.variant == nullptr) {
            Error::internal("variant is nullptr");
          }
          verifyLocal(selector.binding);
        } else if constexpr (std::is_same_v<T, HIRWildcardCase>) {
          // wildcard 처리
        }
      },
      pattern->selector);
}

bool HIRVerifier::definitelyReturns(HIRStmt *stmt) {
  switch (stmt->kind) {
  case HIRNodeKind::ReturnStmt:
    return true;

  case HIRNodeKind::BlockStmt: {
    auto block = expect<HIRBlockStmt>(stmt, HIRNodeKind::BlockStmt);
    for (auto &s : block->statements) {
      if (definitelyReturns(s.get()))
        return true;
    }
    return false;
  }

  case HIRNodeKind::IfStmt: {
    auto ifs = expect<HIRIfStmt>(stmt, HIRNodeKind::IfStmt);
    if (!ifs->elseBlock)
      return false;
    return definitelyReturns(ifs->thenBlock.get()) &&
           definitelyReturns(ifs->elseBlock.get());
  }

  case HIRNodeKind::SwitchStmt: {
    auto sw = expect<HIRSwitchStmt>(stmt, HIRNodeKind::SwitchStmt);

    bool hasDefault = false;

    for (auto &c : sw->cases) {
      if (c->defaultKind == HIRDefaultKind::Default)
        hasDefault = true;

      if (!definitelyReturns(c->body.get()))
        return false;
    }

    return hasDefault;
  }

  case HIRNodeKind::WhileStmt:
    return false; // 기본은 안전하게 false

  default:
    return false;
  }
}