#pragma once

#include "hrd/IR/MIR/MIRNode.h"

using namespace std;

struct MIRProgram {
  vector<unique_ptr<MIRFunction>> functions;
};