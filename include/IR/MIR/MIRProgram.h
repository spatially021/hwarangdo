#pragma once

#include "IR/MIR/MIRNode.h"
#include "IR/MIR/MIRType.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"

using namespace std;

struct MIRProgram {
  vector<unique_ptr<MIRFunction>> functions;
  vector<unique_ptr<MIRType>> types;
  unordered_map<TypeSymbol *, MIRType *> typeMap;
};