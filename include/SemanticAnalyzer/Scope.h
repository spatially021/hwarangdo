#pragma once

#include "Symbol.h"
#include <memory>
#include <string>
#include <vector>

using std::string;
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