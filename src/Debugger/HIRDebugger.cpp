#include "hrd/Debugger/HIRDebugger.h"
#include "hrd/Debugger/DebuggerUtil.h"
#include "hrd/IR/HIR/HIRDecl.h"
#include "hrd/IR/HIR/HIRProgram.h"
#include "hrd/IR/HIR/HIRSymbol.h"
#include "hrd/enums/MethodKind.h"
#include "magic_enum/magic_enum.hpp"
#include <iostream>
#include <type_traits>

using namespace std;

HIRDebugger::HIRDebugger(HIRProgram *p) : program(p) {}

string HIRDebugger::ident() { return string(depth * 2, ' '); }

void HIRDebugger::debug() {
  cout << "[HIRProgram]\n";
  depth++;
  for (auto &t : program->typeDeclMap) {
    debugType(t.second);
  }
  depth--;
}

void HIRDebugger::debugType(HIRTypeDecl *type) {
  cout << ident();
  cout << magic_enum::enum_name(type->typeDeclKind) << " ";
  cout << type->name;
  cout << "\n";
  depth++;
  for (auto &f : type->fields) {
    debugField(f.get());
  }
  for (auto &m : type->methods) {
    debugMethod(m.get());
  }
  depth--;
}

void HIRDebugger::debugField(HIRField *field) {
  cout << ident();
  cout << "field " << field->name;
  cout << " : " << field->type->name;
  cout << "\n";
}

void HIRDebugger::debugMethod(HIRMethodDecl *method) {
  cout << ident();

  if (method->isStatic)
    cout << "static ";
  if (method->isAsync)
    cout << "async ";

  switch (method->methodKind) {

  case MethodKind::Normal: {
    cout << "method ";
    break;
  }
  case MethodKind::Init: {
    cout << "init ";
    break;
  }
  case MethodKind::OnDestroy: {
    cout << "onDestroy ";
    break;
  }
  }

  cout << method->name << "(";

  for (size_t i = 0; i < method->params.size(); i++) {
    auto *p = method->params[i].get();

    if (i != 0)
      cout << ", ";

    cout << p->name;
    cout << " : ";

    if (p->type != nullptr)
      cout << p->type->name;
    else
      cout << "<null-type>";
  }

  cout << ")";

  if (method->methodKind == MethodKind::Normal) {
    cout << " -> ";
    if (method->returnType != nullptr)
      cout << method->returnType->name;
    else
      cout << "<null-return>";
  }

  cout << " [id=" << method->id << "]\n";

  depth++;

  if (!method->locals.empty()) {
    cout << ident() << "locals\n";
    depth++;
    for (auto &l : method->locals) {
      debugLocal(l.get());
    }
    depth--;
  }

  if (method->body != nullptr) {
    cout << ident() << "body\n";
    depth++;
    debugBlock(method->body.get());
    depth--;
  } else {
    cout << ident() << "body <null>\n";
  }

  depth--;
}

void HIRDebugger::debugLocal(HIRLocal *local) {
  cout << ident();

  cout << magic_enum::enum_name(local->kind);
  cout << " ";
  cout << local->name;

  if (local->type != nullptr) {
    cout << " : ";
    cout << local->type->name;
  }

  cout << " [id=" << local->id << "]";

  if (!local->isMutable)
    cout << " const";

  if (local->isInitialized)
    cout << " initialized";

  if (local->isCaseValue)
    cout << " case-value";

  cout << "\n";
}

void HIRDebugger::debugBlock(HIRBlockStmt *block) {
  cout << ident();
  cout << "Block";

  cout << " [stmts=" << block->statements.size();

  if (!block->localMap.empty())
    cout << ", locals=" << block->localMap.size();

  if (block->parent != nullptr)
    cout << ", has-parent";

  cout << "]\n";

  depth++;

  for (auto &stmt : block->statements) {
    if (stmt == nullptr) {
      cout << ident() << "<null-stmt>\n";
      continue;
    }

    debugStmt(stmt.get());
  }

  depth--;
}

void HIRDebugger::debugStmt(HIRStmt *stmt) {
  if (stmt == nullptr) {
    cout << ident() << "<null-stmt>\n";
    return;
  }

  switch (stmt->kind) {
  case HIRNodeKind::BlockStmt:
    debugBlock(static_cast<HIRBlockStmt *>(stmt));
    break;

  case HIRNodeKind::ExprStmt:
    debugExprStmt(static_cast<HIRExprStmt *>(stmt));
    break;

  case HIRNodeKind::LocalDeclStmt:
    debugLocalDeclStmt(static_cast<HIRLocalDeclStmt *>(stmt));
    break;

  case HIRNodeKind::IfStmt:
    debugIfStmt(static_cast<HIRIfStmt *>(stmt));
    break;

  case HIRNodeKind::WhileStmt:
    debugWhileStmt(static_cast<HIRWhileStmt *>(stmt));
    break;

  case HIRNodeKind::ForRangeStmt:
    debugForRangeStmt(static_cast<HIRForRangeStmt *>(stmt));
    break;

  case HIRNodeKind::ReturnStmt:
    debugReturnStmt(static_cast<HIRReturnStmt *>(stmt));
    break;

  case HIRNodeKind::BreakStmt:
    cout << ident() << "Break\n";
    break;

  case HIRNodeKind::ContinueStmt:
    cout << ident() << "Continue\n";
    break;

  case HIRNodeKind::Case:
    debugCase(static_cast<HIRCase *>(stmt));
    break;

  case HIRNodeKind::SwitchStmt:
    debugSwitchStmt(static_cast<HIRSwitchStmt *>(stmt));
    break;

  case HIRNodeKind::ValueTransferStmt:
    debugValueTransferStmt(static_cast<HIRValueTransferStmt *>(stmt));
    break;

  case HIRNodeKind::DestroyStmt:
    debugDestroyStmt(static_cast<HIRDestroyStmt *>(stmt));
    break;

  case HIRNodeKind::QuitStmt:
    cout << ident() << "Quit\n";
    break;

  case HIRNodeKind::AssignStmt:
    debugAssignStmt(static_cast<HIRAssignStmt *>(stmt));
    break;

  case HIRNodeKind::CompoundAssignStmt:
    debugCompoundAssignStmt(static_cast<HIRCompoundAssignStmt *>(stmt));
    break;

  default:
    cout << ident() << "<unknown-stmt ";
    cout << magic_enum::enum_name(stmt->kind);
    cout << ">\n";
    break;
  }
}

void HIRDebugger::debugExprStmt(HIRExprStmt *stmt) {
  cout << ident() << "ExprStmt\n";
  depth++;
  debugExpr(stmt->expr.get());
  depth--;
}

void HIRDebugger::debugLocalDeclStmt(HIRLocalDeclStmt *stmt) {
  cout << ident() << "LocalDecl\n";
  depth++;

  if (stmt->local != nullptr) {
    debugLocal(stmt->local);
  } else {
    cout << ident() << "<null-local>\n";
  }

  if (stmt->init != nullptr) {
    cout << ident() << "init\n";
    depth++;
    debugExpr(stmt->init.get());
    depth--;
  }

  depth--;
}

void HIRDebugger::debugIfStmt(HIRIfStmt *stmt) {
  cout << ident() << "If\n";
  depth++;

  cout << ident() << "condition\n";
  depth++;
  debugExpr(stmt->condition.get());
  depth--;

  cout << ident() << "then\n";
  depth++;
  debugBlock(stmt->thenBlock.get());
  depth--;

  if (stmt->elseBlock != nullptr) {
    cout << ident() << "else\n";
    depth++;
    debugBlock(stmt->elseBlock.get());
    depth--;
  }

  depth--;
}

void HIRDebugger::debugWhileStmt(HIRWhileStmt *stmt) {
  cout << ident() << "While\n";
  depth++;

  cout << ident() << "condition\n";
  depth++;
  debugExpr(stmt->condition.get());
  depth--;

  cout << ident() << "body\n";
  depth++;
  debugBlock(stmt->body.get());
  depth--;

  depth--;
}

void HIRDebugger::debugForRangeStmt(HIRForRangeStmt *stmt) {
  cout << ident() << "ForRange\n";
  depth++;

  cout << ident() << "index\n";
  depth++;
  debugLocal(stmt->indexVar);
  depth--;

  cout << ident() << "start\n";
  depth++;
  debugExpr(stmt->start.get());
  depth--;

  cout << ident() << "end\n";
  depth++;
  debugExpr(stmt->end.get());
  depth--;

  if (stmt->step != nullptr) {
    cout << ident() << "step\n";
    depth++;
    debugExpr(stmt->step.get());
    depth--;
  } else {
    cout << ident() << "step <default>\n";
  }

  cout << ident() << "body\n";
  depth++;
  debugBlock(stmt->body.get());
  depth--;

  depth--;
}

void HIRDebugger::debugReturnStmt(HIRReturnStmt *stmt) {
  cout << ident() << "Return\n";

  if (stmt->value == nullptr)
    return;

  depth++;
  debugExpr(stmt->value.get());
  depth--;
}

void HIRDebugger::debugSwitchStmt(HIRSwitchStmt *stmt) {
  cout << ident() << "Switch\n";
  depth++;

  cout << ident() << "condition\n";
  depth++;
  debugExpr(stmt->cond.get());
  depth--;

  cout << ident() << "cases\n";
  depth++;
  for (auto &c : stmt->cases) {
    debugCase(c.get());
  }
  depth--;

  depth--;
}

void HIRDebugger::debugCase(HIRCase *stmt) {
  cout << ident() << "Case";

  if (stmt->defaultKind != HIRDefaultKind::None) {
    cout << " ";
    cout << magic_enum::enum_name(stmt->defaultKind);
  }

  cout << "\n";
  depth++;

  if (!stmt->selectors.empty()) {
    cout << ident() << "selectors\n";
    depth++;
    for (auto &s : stmt->selectors) {
      debugCasePattern(s.get());
    }
    depth--;
  }

  cout << ident() << "body\n";
  depth++;
  debugBlock(stmt->body.get());
  depth--;

  depth--;
}

void HIRDebugger::debugValueTransferStmt(HIRValueTransferStmt *stmt) {
  cout << ident() << "ValueTransfer\n";
  depth++;
  debugExpr(stmt->value.get());
  depth--;
}

void HIRDebugger::debugDestroyStmt(HIRDestroyStmt *stmt) {
  cout << ident() << "Destroy";
  cout << " storage=" << magic_enum::enum_name(stmt->storage);

  if (stmt->entity != nullptr)
    cout << " entity=" << stmt->entity->name;
  else
    cout << " entity=<null>";

  cout << "\n";

  depth++;
  cout << ident() << "handle\n";
  depth++;
  debugExpr(stmt->handle.get());
  depth--;
  depth--;
}

void HIRDebugger::debugAssignStmt(HIRAssignStmt *stmt) {
  cout << ident() << "Assign\n";
  depth++;

  cout << ident() << "lhs\n";
  depth++;
  debugExpr(stmt->lhs.get());
  depth--;

  cout << ident() << "rhs\n";
  depth++;
  debugExpr(stmt->rhs.get());
  depth--;

  depth--;
}

void HIRDebugger::debugCompoundAssignStmt(HIRCompoundAssignStmt *stmt) {
  cout << ident() << "CompoundAssign";
  cout << " op=" << magic_enum::enum_name(stmt->op);
  cout << "\n";

  depth++;

  cout << ident() << "lhs\n";
  depth++;
  debugExpr(stmt->lhs.get());
  depth--;

  cout << ident() << "rhs\n";
  depth++;
  debugExpr(stmt->rhs.get());
  depth--;

  depth--;
}

void HIRDebugger::debugExpr(HIRExpr *expr) {
  if (expr == nullptr) {
    cout << ident() << "<null-expr>\n";
    return;
  }

  auto printHeader = [&]() {
    cout << ident() << magic_enum::enum_name(expr->kind);

    if (expr->type != nullptr)
      cout << " : " << expr->type->name;
    else
      cout << " : <null-type>";

    cout << " [" << magic_enum::enum_name(expr->category) << "]";
    cout << "\n";
  };

  switch (expr->kind) {
  case HIRNodeKind::LocalPlaceExpr: {
    auto *e = static_cast<HIRLocalPlaceExpr *>(expr);
    cout << ident() << "LocalPlace ";
    if (e->local != nullptr)
      cout << e->local->name << " : " << e->local->type->name
           << " [id=" << e->local->id << "]";
    else
      cout << "<null-local>";
    cout << "\n";
    break;
  }

  case HIRNodeKind::ParamPlaceExpr: {
    auto *e = static_cast<HIRParamPlaceExpr *>(expr);
    cout << ident() << "ParamPlace ";
    if (e->param != nullptr)
      cout << e->param->name << " : " << e->param->type->name
           << " [id=" << e->param->id << "]";
    else
      cout << "<null-param>";
    cout << "\n";
    break;
  }

  case HIRNodeKind::FieldPlaceExpr: {
    auto *e = static_cast<HIRFieldPlaceExpr *>(expr);
    cout << ident() << "FieldPlace ";
    if (e->field != nullptr)
      cout << e->field->name << " : " << e->field->type->name;
    else
      cout << "<null-field>";
    cout << "\n";

    depth++;
    cout << ident() << "receiver\n";
    depth++;
    debugExpr(e->receiver.get());
    depth--;
    depth--;
    break;
  }

  case HIRNodeKind::ArrayAccessExpr: {
    auto *e = static_cast<HIRArrayAccessPlaceExpr *>(expr);
    cout << ident() << "ArrayAccess";
    if (e->elementType != nullptr)
      cout << " : " << e->elementType->name;
    cout << "\n";

    depth++;
    cout << ident() << "object\n";
    depth++;
    debugExpr(e->object.get());
    depth--;

    cout << ident() << "index\n";
    depth++;
    debugExpr(e->index.get());
    depth--;
    depth--;
    break;
  }

  case HIRNodeKind::LoadExpr: {
    auto *e = static_cast<HIRLoadExpr *>(expr);
    cout << ident() << "Load";
    if (e->type != nullptr)
      cout << " : " << e->type->name;
    cout << "\n";

    depth++;
    debugExpr(e->place.get());
    depth--;
    break;
  }

  case HIRNodeKind::LiteralExpr: {
    auto *e = static_cast<HIRLiteralExpr *>(expr);
    cout << ident() << "Literal";

    if (e->type != nullptr)
      cout << " : " << e->type->name;

    cout << " ";
    DebugUtil::debugLiteral(e->resolvedLit);
    cout << "\n";
    break;
  }

  case HIRNodeKind::UnaryExpr: {
    auto *e = static_cast<HIRUnaryExpr *>(expr);
    cout << ident() << "Unary";
    cout << " op=" << magic_enum::enum_name(e->op);
    if (e->type != nullptr)
      cout << " : " << e->type->name;
    cout << "\n";

    depth++;
    debugExpr(e->operand.get());
    depth--;
    break;
  }

  case HIRNodeKind::BinaryExpr: {
    auto *e = static_cast<HIRBinaryExpr *>(expr);
    cout << ident() << "Binary";
    cout << " op=" << magic_enum::enum_name(e->op);
    if (e->type != nullptr)
      cout << " : " << e->type->name;
    cout << "\n";

    depth++;
    cout << ident() << "left\n";
    depth++;
    debugExpr(e->left.get());
    depth--;

    cout << ident() << "right\n";
    depth++;
    debugExpr(e->right.get());
    depth--;
    depth--;
    break;
  }

  case HIRNodeKind::TernaryExpr: {
    auto *e = static_cast<HIRTernaryExpr *>(expr);
    cout << ident() << "Ternary";
    if (e->type != nullptr)
      cout << " : " << e->type->name;
    cout << "\n";

    depth++;
    cout << ident() << "condition\n";
    depth++;
    debugExpr(e->condition.get());
    depth--;

    cout << ident() << "then\n";
    depth++;
    debugExpr(e->thenExpr.get());
    depth--;

    cout << ident() << "else\n";
    depth++;
    debugExpr(e->elseExpr.get());
    depth--;
    depth--;
    break;
  }

  case HIRNodeKind::CastExpr: {
    auto *e = static_cast<HIRCastExpr *>(expr);
    cout << ident() << "Cast ";

    if (e->fromType != nullptr)
      cout << e->fromType->name;
    else
      cout << "<null-from>";

    cout << " -> ";

    if (e->toType != nullptr)
      cout << e->toType->name;
    else
      cout << "<null-to>";

    cout << "\n";

    depth++;
    debugExpr(e->operand.get());
    depth--;
    break;
  }

  case HIRNodeKind::MethodCallExpr: {
    auto *e = static_cast<HIRMethodCallExpr *>(expr);
    cout << ident() << "MethodCall ";

    if (e->method != nullptr)
      cout << e->method->name;
    else
      cout << "<null-method>";

    if (e->type != nullptr)
      cout << " : " << e->type->name;

    cout << "\n";

    depth++;

    if (e->receiver != nullptr) {
      cout << ident() << "receiver\n";
      depth++;
      debugExpr(e->receiver.get());
      depth--;
    }

    cout << ident() << "args\n";
    depth++;
    for (auto &arg : e->args)
      debugExpr(arg.get());
    depth--;

    depth--;
    break;
  }

  case HIRNodeKind::StructInitExpr: {
    auto *e = static_cast<HIRStructInitExpr *>(expr);
    cout << ident() << "StructInit";

    if (e->method != nullptr)
      cout << " init=" << e->method->name;
    else
      cout << " init=<default>";

    if (e->type != nullptr)
      cout << " : " << e->type->name;

    if (e->isDefault)
      cout << " default";

    cout << "\n";

    depth++;
    cout << ident() << "args\n";
    depth++;
    for (auto &arg : e->args)
      debugExpr(arg.get());
    depth--;
    depth--;
    break;
  }

  case HIRNodeKind::SpawnExpr: {
    auto *e = static_cast<HIRSpawnExpr *>(expr);
    cout << ident() << "Spawn";
    cout << " storage=" << magic_enum::enum_name(e->storage);

    if (e->entityType != nullptr)
      cout << " entity=" << e->entityType->name;
    else
      cout << " entity=<null>";

    if (e->type != nullptr)
      cout << " : " << e->type->name;

    cout << "\n";

    depth++;
    cout << ident() << "args\n";
    depth++;
    for (auto &arg : e->args)
      debugExpr(arg.get());
    depth--;
    depth--;
    break;
  }

  case HIRNodeKind::ViewExpr: {
    auto *e = static_cast<HIRViewExpr *>(expr);
    cout << ident() << "View";
    cout << " storage=" << magic_enum::enum_name(e->storage);

    if (e->entityType != nullptr)
      cout << " entity=" << e->entityType->name;
    else
      cout << " entity=<null>";

    if (e->type != nullptr)
      cout << " : " << e->type->name;

    cout << "\n";

    depth++;
    cout << ident() << "handle\n";
    depth++;
    debugExpr(e->handle.get());
    depth--;
    depth--;
    break;
  }

  case HIRNodeKind::MatchExpr: {
    auto *e = static_cast<HIRMatchExpr *>(expr);
    cout << ident() << "Match";
    if (e->type != nullptr)
      cout << " : " << e->type->name;
    cout << "\n";

    depth++;

    cout << ident() << "condition\n";
    depth++;
    debugExpr(e->cond.get());
    depth--;

    cout << ident() << "cases\n";
    depth++;
    for (auto &c : e->cases)
      debugCase(c.get());
    depth--;

    depth--;
    break;
  }

  case HIRNodeKind::EnumVariantValue: {
    auto *e = static_cast<HIRVariantValueExpr *>(expr);
    cout << ident() << "EnumVariantValue ";

    if (e->varaint != nullptr)
      cout << e->varaint->name;
    else
      cout << "<null-variant>";

    if (e->type != nullptr)
      cout << " : " << e->type->name;

    cout << "\n";

    if (e->payload != nullptr) {
      depth++;
      cout << ident() << "payload\n";
      depth++;
      debugExpr(e->payload.get());
      depth--;
      depth--;
    }

    break;
  }

  case HIRNodeKind::SelfExpr: {
    auto *e = static_cast<HIRSelfExpr *>(expr);
    cout << ident() << magic_enum::enum_name(e->selfKind);

    if (e->type != nullptr)
      cout << " : " << e->type->name;

    if (e->ownerType != nullptr)
      cout << " owner=" << e->ownerType->name;

    if (e->accessType != nullptr)
      cout << " access=" << e->accessType->name;

    cout << "\n";
    break;
  }

  case HIRNodeKind::RootExpr: {
    cout << ident() << "Root";
    if (expr->type != nullptr)
      cout << " : " << expr->type->name;
    cout << "\n";
    break;
  }

  case HIRNodeKind::DefaultValueExpr: {
    cout << ident() << "DefaultValue";
    if (expr->type != nullptr)
      cout << " : " << expr->type->name;
    cout << "\n";
    break;
  }

  default:
    printHeader();
    break;
  }
}

void HIRDebugger::debugCasePattern(HIRCasePattern *pattern) {
  if (pattern == nullptr) {
    cout << ident() << "<null-pattern>\n";
    return;
  }

  std::visit(
      [&](auto &&selector) {
        using T = std::decay_t<decltype(selector)>;

        if constexpr (std::is_same_v<T, HIRLiteralCase>) {
          cout << ident() << "LiteralCase\n";
          depth++;
          debugExpr(selector.expr.get());
          depth--;
        }

        else if constexpr (std::is_same_v<T, HIRUnitCase>) {
          cout << ident() << "UnitCase ";

          if (selector.variant != nullptr)
            cout << selector.variant->name;
          else
            cout << "<null-variant>";

          cout << "\n";
        }

        else if constexpr (std::is_same_v<T, HIRPayloadCase>) {
          cout << ident() << "PayloadCase ";

          if (selector.variant != nullptr)
            cout << selector.variant->name;
          else
            cout << "<null-variant>";

          cout << "\n";

          depth++;
          cout << ident() << "binding\n";
          depth++;
          debugLocal(selector.binding);
          depth--;
          depth--;
        }

        else if constexpr (std::is_same_v<T, HIRWildcardCase>) {
          cout << ident() << "WildcardCase\n";
        }
      },
      pattern->selector);
}
