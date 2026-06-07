#pragma once

#include "IR/MIR/MIRExpr.h"
#include "IR/MIR/MIRInst.h"
#include "SemanticAnalyzer/symbol/MethodSymbol.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "util/Error.h"
#include <cstdint>
#include <memory>
#include <utility>
#include <variant>

using BlockID = uint64_t;
using namespace std;
enum class TerminatorKind {
  Invalid,
  Goto,
  Branch,
  Return,
  Switch,
};

struct GotoTerminator {
  BlockID targetBlock;
  GotoTerminator(BlockID ta) : targetBlock(ta) {}
};

struct BranchTerminator {
  unique_ptr<MIRExpr> cond;
  BlockID trueBlock;
  BlockID falseBlock;
  BranchTerminator(unique_ptr<MIRExpr> co, BlockID tr, BlockID fa)
      : cond(std::move(co)), trueBlock(tr), falseBlock(fa) {}
};

struct ReturnTerminator {
  // void return이면 비워도 됨
  // 값 반환 지원하려면 나중에 optional<MIRValue> value;
};

struct SwitchTerminator {
  // 나중에 채우기
};

using MIRTerminator = variant<std::monostate, GotoTerminator, BranchTerminator,
                              ReturnTerminator, SwitchTerminator>;

struct BasicBlock {
  BlockID id = 0;
  MIRTerminator terminator;
  vector<unique_ptr<MIRInst>> insts;
  BasicBlock(BlockID i) { id = i; }
};

class MIRFunction {
public:
  vector<unique_ptr<BasicBlock>> blocks;
  MethodSymbol *symbol = nullptr;
  TypeSymbol *onwer = nullptr;
  BlockID entry;

  uint32_t nextTemp = 0;

  MIRFunction(MethodSymbol *s, TypeSymbol *o) : symbol(s), onwer(o) {}

  BlockID createBlock() {
    BlockID id = blocks.size();

    blocks.push_back(make_unique<BasicBlock>(id));

    return id;
  }

  BasicBlock *getBlock(BlockID id) {
    if (id > blocks.size()) {
      Error::internal("unknown id");
    } else {
      return blocks[id].get();
    }
  }
};