#pragma once

#include "IR/MIR/MIRExpr.h"
#include <memory>
#include <utility>
using namespace std;

struct MIRInst {};

struct MIRExprStmtInst : MIRInst {
  unique_ptr<MIRExpr> expr = nullptr;
  MIRExprStmtInst(unique_ptr<MIRExpr> e) : expr(std::move(e)) {}
};