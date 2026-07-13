#pragma once

#include "hrd/IR/MIR/MIRExpr.h"
#include "hrd/IR/MIR/MIRStmt.h"
#include "hrd/SemanticAnalyzer/ResolvedLit.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/util/Error.h"
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
  unique_ptr<MIRValue> cond;
  BlockID trueBlock;
  BlockID falseBlock;
  BranchTerminator(unique_ptr<MIRValue> co, BlockID tr, BlockID fa)
      : cond(std::move(co)), trueBlock(tr), falseBlock(fa) {}
};

struct ReturnTerminator {
  unique_ptr<MIRValue> value = nullptr;
  ReturnTerminator(unique_ptr<MIRValue> v) : value(std::move(v)) {}
};

using MIRCaseValue = std::variant<ResolvedLit, EnumVariantSymbol *>;
struct MIRCase {
  MIRCaseValue value;
  BlockID target;
  MIRCase(ResolvedLit v, BlockID t) : value(v), target(t) {}
  MIRCase(EnumVariantSymbol *v, BlockID t) : value(v), target(t) {}
};

struct SwitchTerminator {
  unique_ptr<MIRValue> cond;
  vector<MIRCase> cases;
  BlockID defaultTarget;
  SwitchTerminator(unique_ptr<MIRValue> c, vector<MIRCase> ca, BlockID d)
      : cond(std::move(c)), cases(std::move(ca)), defaultTarget(d) {}
};

using MIRTerminator = variant<std::monostate, GotoTerminator, BranchTerminator,
                              ReturnTerminator, SwitchTerminator>;

struct BasicBlock {
  BlockID id = 0;
  MIRTerminator terminator;
  vector<unique_ptr<MIRStmt>> stmts;
  BasicBlock(BlockID i) { id = i; }
};

class MIRFunction {
public:
  vector<unique_ptr<BasicBlock>> blocks;
  MethodSymbol *symbol = nullptr;
  TypeSymbol *owner = nullptr;
  BlockID entry;

  uint32_t nextTemp = 0;

  bool isDefaultInit = false;

  MIRFunction(MethodSymbol *s, TypeSymbol *o) : symbol(s), owner(o) {}

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