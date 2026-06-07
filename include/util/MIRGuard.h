#pragma once

// class BoolGuard {
// private:
//   bool &slot;
//   bool prev;

// public:
//   BoolGuard(bool &s, bool next) : slot(s), prev(s) { slot = next; }
//   ~BoolGuard() { slot = prev; }
// };

#include "IR/MIR/MIRNode.h"
class FuncGuard {

private:
  MIRFunction *&slot;
  MIRFunction *prev = nullptr;

public:
  FuncGuard(MIRFunction *&currnet, MIRFunction *next)
      : slot(currnet), prev(currnet) {
    slot = next;
  }
  ~FuncGuard() { slot = prev; }
};
