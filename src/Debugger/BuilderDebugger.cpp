#include "Debugger/BuilderDebugger.h"
#include "SemanticAnalyzer/symbol/MethodSymbol.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "SemanticAnalyzer/symbol/ValueSymbol.h"
#include <iostream>
#include <string>

using std::cout;
using std::string;

string BuilderDebugger::ident() { return string(depth * 2, ' '); }
BuilderDebugger::BuilderDebugger(Scope *s) : toplevel(s) {}
void BuilderDebugger::debug() {
  debug(toplevel->parent);
  debug(toplevel);
}
void BuilderDebugger::debug(Scope *scope) {
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

  if (!scope->value.empty()) {
    cout << ident() << "<values>\n";
    depth++;
    for (auto &v : scope->value) {
      cout << ident() << "-" << " " << v.second->name << "\n";
    }
    depth--;
  }

  if (!scope->methodMap.empty()) {
    cout << ident() << "<methods>\n";
    depth++;
    for (auto &m : scope->methodMap) {
      if (m.second == nullptr)
        continue;
      cout << ident() << m.second->name << "\n";
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
