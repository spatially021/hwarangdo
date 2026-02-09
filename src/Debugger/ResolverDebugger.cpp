#include "Debugger/ResolverDebugger.h"
#include <iostream>
#include <string>

using std::cout;
using std::string;

string ResolverDebugger::ident() { return string(depth * 2, ' '); }

ResolverDebugger::ResolverDebugger(Scope *scope) : toplevel(scope){}

void ResolverDebugger::debug() { debug(toplevel); }

void ResolverDebugger::debug(Scope *scope) {
  // scope header
  cout << ident() << "[scope#" << scope->id << "]\n";
  depth++;

  // resolutions (values)
  if (!scope->value.empty()) {
    cout << ident() << "<resolutions>\n";
    depth++;
    for (auto &v : scope->value) {
      cout<<ident()<<v.second->name<<" [line : "<<v.second->node->token.line<<"] -";
      if (v.second->typeSymbol) {
        cout << v.second->typeSymbol->name;
      } else {
        cout << "<unresolved>";
      }
      cout<<"\n";
    }
    depth--;
  }

  // methods
  if (!scope->method.empty()) {
    cout << ident() << "<methods>\n";
    depth++;
    for (auto &m : scope->method) {
      cout << ident() << m.second->name << "\n";
    }
    depth--;
  }

  // types
  if (!scope->type.empty()) {
    cout << ident() << "<types>\n";
    depth++;
    for (auto &t : scope->type) {
      cout << ident() << t.second->name << "\n";
    }
    depth--;
  }

  // children
  for (auto &child : scope->children) {
    debug(child.get());
  }

  depth--;
}
