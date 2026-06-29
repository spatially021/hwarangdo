#include "hrd/Debugger/MIRDebugger/MIRDebuuger.h"
#include "hrd/Debugger/DebuggerUtil.h"
#include "hrd/IR/MIR/MIRProgram.h"
#include "magic_enum/magic_enum.hpp"
#include <iostream>

using namespace std;

MIRDebugger::MIRDebugger(MIRProgram *p) : program(p) {}

string MIRDebugger::ident() { return string(depth * 2, ' '); }

void MIRDebugger::debug() {
  cout << "[MIRProgram]\n";

  depth++;
  for (auto &func : program->functions) {
    debugFunction(func.get());
  }
  depth--;
}

void MIRDebugger::debugFunction(MIRFunction *func) {
  cout << ident() << "function ";

  if (func->onwer != nullptr)
    cout << func->onwer->name << ".";

  if (func->symbol != nullptr)
    cout << func->symbol->name;
  else
    cout << "<null-symbol>";

  cout << " [entry=#" << func->entry << "]\n";

  depth++;

  cout << ident() << "blocks\n";
  depth++;
  for (auto &block : func->blocks) {
    debugBlock(block.get());
  }
  depth--;

  depth--;
}

void MIRDebugger::debugBlock(BasicBlock *block) {
  cout << ident() << "block #" << block->id << "\n";

  depth++;

  if (!block->stmts.empty()) {
    cout << ident() << "stmts\n";
    depth++;
    for (auto &stmt : block->stmts) {
      debugStmt(stmt.get());
    }
    depth--;
  }

  cout << ident() << "terminator\n";
  depth++;
  debugTerminator(block->terminator);
  depth--;

  depth--;
}

void MIRDebugger::debugTerminator(const MIRTerminator &term) {
  std::visit(
      [&](auto &&t) {
        using T = std::decay_t<decltype(t)>;

        if constexpr (std::is_same_v<T, std::monostate>) {
          cout << ident() << "<none>\n";
        }

        else if constexpr (std::is_same_v<T, GotoTerminator>) {
          cout << ident() << "goto #" << t.targetBlock << "\n";
        }

        else if constexpr (std::is_same_v<T, BranchTerminator>) {
          cout << ident() << "branch\n";

          depth++;
          cout << ident() << "cond\n";
          depth++;
          debugValue(t.cond.get());
          depth--;

          cout << ident() << "true #" << t.trueBlock << "\n";
          cout << ident() << "false #" << t.falseBlock << "\n";
          depth--;
        }

        else if constexpr (std::is_same_v<T, ReturnTerminator>) {
          cout << ident() << "return\n";

          if (t.value != nullptr) {
            depth++;
            debugValue(t.value.get());
            depth--;
          }
        }

        else if constexpr (std::is_same_v<T, SwitchTerminator>) {
          cout << ident() << "switch\n";

          depth++;

          cout << ident() << "cond\n";
          depth++;
          debugValue(t.cond.get());
          depth--;

          cout << ident() << "cases\n";
          depth++;
          for (auto &c : t.cases) {
            debugCase(c);
          }
          depth--;

          cout << ident() << "default #" << t.defaultTarget << "\n";

          depth--;
        }
      },
      term);
}

void MIRDebugger::debugCase(const MIRCase &c) {
  cout << ident() << "case ";

  std::visit(
      [&](auto &&v) {
        using T = std::decay_t<decltype(v)>;

        if constexpr (std::is_same_v<T, ResolvedLit>) {
          DebugUtil::debugLiteral(v);
        }

        else if constexpr (std::is_same_v<T, EnumVariantSymbol *>) {
          if (v != nullptr)
            cout << v->name;
          else
            cout << "<null-variant>";
        }
      },
      c.value);

  cout << " -> #" << c.target << "\n";
}

void MIRDebugger::debugStmt(MIRStmt *stmt) {
  if (stmt == nullptr) {
    cout << ident() << "<null-stmt>\n";
    return;
  }

  if (auto *s = dynamic_cast<MIRExprStmt *>(stmt)) {
    cout << ident() << "ExprStmt\n";
    depth++;
    debugValue(s->expr.get());
    depth--;
    return;
  }

  if (auto *s = dynamic_cast<MIRAssignStmt *>(stmt)) {
    cout << ident() << "Assign\n";
    depth++;

    cout << ident() << "lhs\n";
    depth++;
    debugPlace(s->lhs.get());
    depth--;

    cout << ident() << "rhs\n";
    depth++;
    debugValue(s->rhs.get());
    depth--;

    depth--;
    return;
  }

  if (auto *s = dynamic_cast<MIRLocalDeclStmt *>(stmt)) {
    cout << ident() << "LocalDecl ";

    if (s->symbol != nullptr)
      cout << s->symbol->name;
    else
      cout << "<null-symbol>";

    cout << " : ";

    if (s->type != nullptr)
      cout << s->type->name;
    else
      cout << "<null-type>";

    cout << "\n";

    if (s->init != nullptr) {
      depth++;
      cout << ident() << "init\n";
      depth++;
      debugValue(s->init.get());
      depth--;
      depth--;
    }

    return;
  }

  if (dynamic_cast<MIRQuitStmt *>(stmt)) {
    cout << ident() << "Quit\n";
    return;
  }

  if (auto *s = dynamic_cast<MIRDestroyStmt *>(stmt)) {
    cout << ident() << "Destroy\n";
    depth++;
    cout << ident() << "handle\n";
    depth++;
    debugValue(s->handlePlace.get());
    depth--;
    depth--;
    return;
  }

  cout << ident() << "<unknown-stmt>\n";
}

void MIRDebugger::debugPlace(MIRPlace *place) {
  if (place == nullptr) {
    cout << ident() << "<null-place>\n";
    return;
  }

  if (auto *p = dynamic_cast<MIRLocalPlace *>(place)) {
    cout << ident() << "LocalPlace ";
    if (p->symbol != nullptr)
      cout << p->symbol->name;
    else
      cout << "<null-symbol>";
    cout << "\n";
    return;
  }

  if (auto *p = dynamic_cast<MIRParamPlace *>(place)) {
    cout << ident() << "ParamPlace ";
    if (p->symbol != nullptr)
      cout << p->symbol->name;
    else
      cout << "<null-symbol>";
    cout << "\n";
    return;
  }

  if (auto *p = dynamic_cast<MIRFieldPlace *>(place)) {
    cout << ident() << "FieldPlace ";
    if (p->symbol != nullptr)
      cout << p->symbol->name;
    else
      cout << "<null-field>";
    cout << "\n";

    depth++;
    cout << ident() << "base\n";
    depth++;
    debugPlace(p->base.get());
    depth--;
    depth--;

    return;
  }

  if (auto *p = dynamic_cast<MIRArrayAccessPlace *>(place)) {
    cout << ident() << "ArrayAccessPlace\n";
    depth++;

    cout << ident() << "base\n";
    depth++;
    debugPlace(p->base.get());
    depth--;

    cout << ident() << "index\n";
    depth++;
    debugValue(p->index.get());
    depth--;

    depth--;
    return;
  }

  if (dynamic_cast<MIRRootPlace *>(place)) {
    cout << ident() << "RootPlace\n";
    return;
  }

  cout << ident() << "<unknown-place>\n";
}

void MIRDebugger::debugValue(MIRValue *value) {
  if (value == nullptr) {
    cout << ident() << "<null-value>\n";
    return;
  }

  if (auto *v = dynamic_cast<MIRLiteralExpr *>(value)) {
    cout << ident() << "Literal ";
    DebugUtil::debugLiteral(v->literal);
    cout << "\n";
    return;
  }

  if (auto *v = dynamic_cast<MIRLoad *>(value)) {
    cout << ident() << "Load\n";
    depth++;
    debugPlace(v->place.get());
    depth--;
    return;
  }

  if (auto *v = dynamic_cast<MIRUnaryExpr *>(value)) {
    cout << ident() << "Unary op=" << magic_enum::enum_name(v->op) << "\n";
    depth++;
    debugValue(v->operrand.get());
    depth--;
    return;
  }

  if (auto *v = dynamic_cast<MIRBinaryExpr *>(value)) {
    cout << ident() << "Binary op=" << magic_enum::enum_name(v->op) << "\n";
    depth++;

    cout << ident() << "lhs\n";
    depth++;
    debugValue(v->lhs.get());
    depth--;

    cout << ident() << "rhs\n";
    depth++;
    debugValue(v->rhs.get());
    depth--;

    depth--;
    return;
  }

  if (auto *v = dynamic_cast<MIRCastExpr *>(value)) {
    cout << ident() << "Cast ";

    cout << (v->from ? v->from->name : "<null-from>");
    cout << " -> ";
    cout << (v->to ? v->to->name : "<null-to>");
    cout << "\n";

    depth++;
    debugValue(v->operrand.get());
    depth--;
    return;
  }

  if (auto *v = dynamic_cast<MIRCallExpr *>(value)) {
    cout << ident() << "Call ";
    cout << (v->method ? v->method->name : "<null-method>");
    cout << "\n";

    depth++;

    cout << ident() << "base\n";
    depth++;
    debugValue(v->base.get());
    depth--;

    cout << ident() << "args\n";
    depth++;
    for (auto &arg : v->args)
      debugValue(arg.get());
    depth--;

    depth--;
    return;
  }

  if (auto *v = dynamic_cast<MIRSpawnExpr *>(value)) {
    cout << ident() << "Spawn ";
    cout << (v->entityType ? v->entityType->name : "<null-entity>");
    if (v->initMethod != nullptr)
      cout << " init=" << v->initMethod->name;
    else
      cout << " init=<default>";
    cout << "\n";

    depth++;
    cout << ident() << "args\n";
    depth++;
    for (auto &arg : v->args)
      debugValue(arg.get());
    depth--;
    depth--;
    return;
  }

  if (auto *v = dynamic_cast<MIRViewExpr *>(value)) {
    cout << ident() << "View ";
    cout << (v->entityType ? v->entityType->name : "<null-entity>");
    cout << "\n";

    depth++;
    cout << ident() << "handle\n";
    depth++;
    debugValue(v->handle.get());
    depth--;
    depth--;
    return;
  }

  if (auto *v = dynamic_cast<MIRStructInitExpr *>(value)) {
    cout << ident() << "StructInit ";
    cout << (v->structType ? v->structType->name : "<null-struct>");
    if (v->initMethod != nullptr)
      cout << " init=" << v->initMethod->name;
    else
      cout << " init=<default>";
    cout << "\n";

    depth++;
    cout << ident() << "args\n";
    depth++;
    for (auto &arg : v->args)
      debugValue(arg.get());
    depth--;
    depth--;
    return;
  }

  if (auto *v = dynamic_cast<MIRVariantExpr *>(value)) {
    cout << ident() << "Variant ";
    cout << (v->variant ? v->variant->name : "<null-variant>");
    cout << "\n";

    if (v->payload != nullptr) {
      depth++;
      cout << ident() << "payload\n";
      depth++;
      debugValue(v->payload.get());
      depth--;
      depth--;
    }

    return;
  }

  if (auto *v = dynamic_cast<MIRPayloadExtractExpr *>(value)) {
    cout << ident() << "PayloadExtract ";
    cout << (v->symbol ? v->symbol->name : "<null-variant>");
    cout << "\n";

    depth++;
    cout << ident() << "enum-value\n";
    depth++;
    debugValue(v->enumValue.get());
    depth--;
    depth--;

    return;
  }

  if (auto *v = dynamic_cast<MIRRuntimeCallExpr *>(value)) {
    cout << ident() << "RuntimeCall ";
    if (v->symbol) {
      cout << v->symbol->namespaceName << "." << v->symbol->name;
      if (!v->symbol->llvmName.empty()) {
        cout << " [" << v->symbol->llvmName << "]";
      }
    } else {
      cout << "<null-runtime>";
    }
    cout << "\n";

    depth++;

    cout << ident() << "args\n";
    depth++;
    for (auto &arg : v->args) {
      debugValue(arg.get());
    }
    depth--;

    depth--;
    return;
  }

  cout << ident() << "<unknown-value>\n";
}