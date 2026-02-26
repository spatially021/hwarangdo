#pragma once

#include "SemanticAnalyzer/Symbol.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::vector;

class Scope {
public:
  Scope *parent = nullptr;
  vector<std::unique_ptr<Scope>> children;

  unordered_map<string, unique_ptr<TypeSymbol>> type;
  unordered_map<string, unique_ptr<ValueSymbol>> value;
  unordered_map<string, unique_ptr<MethodSymbol>> method;
  int id = 0;
  string name = "";
};