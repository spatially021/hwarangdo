#include "Debugger/BuilderDebugger.h"
#include <iostream>
#include <string>

using std::cout;
using std::string;

string BuilderDebugger::ident() { return string(depth * 2, ' '); }
BuilderDebugger::BuilderDebugger(Scope *s) : toplevel(s) {}
void BuilderDebugger::debug() { debug(toplevel); }
void BuilderDebugger::debug(Scope *scope) {
  cout << ident() << "[scope#" << scope->id << "]\n";
  depth++;

  if (!scope->value.empty()) {
    cout << ident() << "<values>\n";
    depth++;
    for (auto &v : scope->value) {
      cout << ident() << "-" << " " << v.second->name << "\n";
    }
    depth--;
  }

  if (!scope->method.empty()) {
    cout << ident() << "<methods>\n";
    depth++;
    for (auto &m : scope->method) {
      if(m.second==nullptr) continue;
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

  for (auto &s : scope->children) {
    debug(s.get());
  }
  depth--;
}
