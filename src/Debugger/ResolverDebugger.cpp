#include "Debugger/ResolverDebugger.h"
#include "SemanticAnalyzer/symbol/MethodSymbol.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "SemanticAnalyzer/symbol/ValueSymbol.h"
#include "magic_enum/magic_enum.hpp"
#include "util/Error.h"
#include <iostream>
#include <string>

using std::cout;
using std::string;

string ResolverDebugger::ident() { return string(depth * 2, ' '); }

ResolverDebugger::ResolverDebugger(Scope *scope) : toplevel(scope) {}

void ResolverDebugger::debug() {
  debug(toplevel->parent);
  debug(toplevel);
}

void ResolverDebugger::debug(Scope *scope) {
  // scope header
  if (scope->id == -1) {
    cout << ident() << "[scope#" << "root" << "]\n";
    depth++;
  } else if (scope->id == -2) {
    cout << ident() << "[scope#" << "reserved" << "]\n";
    depth++;
  } else {
    cout << ident() << "[scope#" << scope->id << "]\n";
    depth++;
  }

  // // resolutions (values)
  // if (!scope->value.empty()) {
  //   cout << ident() << "<resolutions>\n";
  //   depth++;
  //   for (auto &v : scope->value) {
  //     cout << ident() << v.second->name
  //          << " [line : " << v.second->node->token.line << "] - ";
  //     if (v.second->typeSymbol) {
  //       cout << v.second->typeSymbol->name;
  //     } else {
  //       cout << "<unresolved>";
  //     }
  //     cout << "\n";
  //   }
  //   depth--;
  // }

  // // methods
  // if (!scope->method.empty()) {
  //   cout << ident() << "<methods>\n";
  //   depth++;
  //   for (auto &m : scope->method) {
  //     cout << ident() << m.second->name << "\n";
  //   }
  //   depth--;
  // }

  // // types
  // if (!scope->type.empty()) {
  //   cout << ident() << "<types>\n";
  //   depth++;
  //   for (auto &t : scope->type) {
  //     cout << ident() << t.second->name << "\n";
  //   }
  //   depth--;
  // }

  // // children
  // for (auto &child : scope->children) {
  //   debug(child.get());
  // }

  // depth--;

  if (!scope->value.empty()) {
    cout << ident() << "<values>\n";
    depth++;
    cout << ident() << "<resolutions>\n";
    depth++;
    for (auto &v : scope->value) {
      if (!v.second->node) {
        Error::internal("uninited node");
      }
      cout << ident() << v.second->name
           << " [line : " << v.second->node->span.lineStart << "] - ";
      if (v.second->typeSymbol) {
        if (v.second->typeSymbol->name == "") {
          cout << magic_enum::enum_name(v.second->typeSymbol->kind);
        } else {
          cout << v.second->typeSymbol->name;
        }

      } else {
        cout << "<unresolved>";
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
        cout << ident() << m->name << "\n";
      }
    }
    depth--;
  }

  if (!scope->type.empty()) {
    cout << ident() << "<types>\n";
    depth++;
    for (auto &t : scope->type) {
      cout << ident() << t.second->name << "\n";
    }
    depth--;
  }

  if (!scope->inits.empty()) {
    cout << ident() << "<inits>\n";
    depth++;
    for (auto &t : scope->inits) {
      cout << ident() << t.second->name << "\n";
    }
    depth--;
  }

  for (auto &s : scope->children) {
    debug(s.get());
  }
  depth--;
}
