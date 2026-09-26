#pragma once

#include "hrd/IR/MIR/MIRNode.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"

using namespace std;

struct MIRGenericTypeDecl {
  TypeSymbol *symbol = nullptr;
  MIRFunction *defaultInit = nullptr;
  vector<MIRFunction *> methods;
  unordered_map<MethodSymbol *, MIRFunction *> methodMap;
};

struct MIRProgram {
  vector<unique_ptr<MIRFunction>> functions;
  vector<unique_ptr<MIRFunction>> genericOrigin;
  unordered_map<TypeSymbol *, MIRGenericTypeDecl> genericMap;
};