#include "hrd/Debugger/ResolverDebugger.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "magic_enum/magic_enum.hpp"
#include <iostream>
#include <string>

using std::cout;
using std::string;

string ResolverDebugger::ident() { return string(depth * 2, ' '); }

ResolverDebugger::ResolverDebugger(Scope *scope) : toplevel(scope) {}
void ResolverDebugger::debug() {
  if (!toplevel) {
    cout << "<null toplevel scope>\n";
    return;
  }

  if (toplevel->parent) {
    debug(toplevel->parent);
  } else {
    cout << "<null toplevel parent>\n";
  }

  debug(toplevel);
}

void ResolverDebugger::debug(Scope *scope) {
  if (!scope) {
    cout << ident() << "<null scope>\n";
    return;
  }

  if (scope->id == -1) {
    cout << ident() << "[scope#root]\n";
  } else if (scope->id == -2) {
    cout << ident() << "[scope#reserved]\n";
  } else {
    cout << ident() << "[scope#" << scope->id << "]\n";
  }

  depth++;

  if (!scope->value.empty()) {
    cout << ident() << "<values>\n";
    depth++;

    cout << ident() << "<resolutions>\n";
    depth++;

    for (auto &v : scope->value) {
      ValueSymbol *value = v.second.get();

      if (!value) {
        cout << ident() << "<null value symbol>\n";
        continue;
      }

      cout << ident();

      if (value->name.empty()) {
        cout << "<unnamed>";
      } else {
        cout << value->name;
      }

      cout << " [line : ";

      if (value->node) {
        cout << value->node->span.lineStart;
      } else {
        cout << "<null node>";
      }

      cout << "] - ";

      if (value->typeSymbol) {
        if (value->typeSymbol->name.empty()) {
          cout << magic_enum::enum_name(value->typeSymbol->kind);
        } else {
          cout << value->typeSymbol->name;
        }
      } else {
        cout << "<null type>";
      }

      cout << "\n";
    }

    depth--;
    depth--;
  }

  if (!scope->methodMap.empty()) {
    cout << ident() << "<methods>\n";
    depth++;

    for (auto &map : scope->methodMap) {
      for (auto &m : map.second) {
        if (!m) {
          cout << ident() << "<null method>\n";
          continue;
        }

        cout << ident();

        if (m->returnType) {
          if (m->returnType->name.empty()) {
            cout << magic_enum::enum_name(m->returnType->kind);
          } else {
            cout << m->returnType->name;
          }
        } else {
          cout << "<null returnType>";
        }

        cout << " ";

        if (m->name.empty()) {
          cout << "<unnamed method>";
        } else {
          cout << m->name;
        }

        cout << "\n";
      }
    }

    depth--;
  }

  if (!scope->type.empty()) {
    cout << ident() << "<types>\n";
    depth++;

    for (auto &t : scope->type) {
      TypeSymbol *type = t.second.get();

      if (!type) {
        cout << ident() << "<null type symbol>\n";
        continue;
      }

      cout << ident();

      if (type->name.empty()) {
        cout << magic_enum::enum_name(type->kind);
      } else {
        cout << type->name;
      }

      cout << "\n";
    }

    depth--;
  }

  if (!scope->inits.empty()) {
    cout << ident() << "<inits>\n";
    depth++;

    for (auto &init : scope->inits) {
      if (!init) {
        cout << ident() << "<null init>\n";
        continue;
      }

      cout << ident();

      if (init->name.empty()) {
        cout << "<unnamed init>";
      } else {
        cout << init->name;
      }

      cout << "\n";
    }

    depth--;
  }

  for (auto &s : scope->children) {
    if (!s) {
      cout << ident() << "<null child scope>\n";
      continue;
    }

    debug(s.get());
  }

  depth--;
}