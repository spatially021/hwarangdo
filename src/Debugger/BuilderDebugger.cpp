#include "hrd/Debugger/BuilderDebugger.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include <iostream>
#include <string>

using std::cout;
using std::string;

string BuilderDebugger::ident() { return string(depth * 2, ' '); }
BuilderDebugger::BuilderDebugger(Scope *s) : toplevel(s) {}
void BuilderDebugger::debug() { debug(toplevel); }
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

  for (auto &s : scope->children) {
    debug(s.get());
  }
  depth--;
}
