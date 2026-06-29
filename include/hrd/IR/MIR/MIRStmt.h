#pragma once

#include "hrd/IR/MIR/MIRExpr.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include <utility>
using namespace std;

struct MIRStmt {
  virtual ~MIRStmt() = default;
};

struct MIRExprStmt : MIRStmt {
  unique_ptr<MIRValue> expr = nullptr;
  MIRExprStmt(unique_ptr<MIRValue> e) : expr(std::move(e)) {}
};

struct MIRAssignStmt : MIRStmt {
  unique_ptr<MIRPlace> lhs = nullptr;
  unique_ptr<MIRValue> rhs = nullptr;

  MIRAssignStmt(unique_ptr<MIRPlace> l, unique_ptr<MIRValue> r)
      : lhs(std::move(l)), rhs(std::move(r)) {}
};

// struct MIRCompoundAssignStmt : MIRStmt {
//   unique_ptr<MIRPlace> lhs = nullptr;
//   unique_ptr<MIRExpr> rhs = nullptr;
//   Operator op;

//   MIRCompoundAssignStmt(unique_ptr<MIRPlace> l, unique_ptr<MIRExpr> r,
//                         Operator o)
//       : lhs(std::move(l)), rhs(std::move(r)), op(o) {}
// };

struct MIRLocalDeclStmt : MIRStmt {
  TypeSymbol *type = nullptr;
  ValueSymbol *symbol = nullptr;
  unique_ptr<MIRValue> init = nullptr;
  MIRLocalDeclStmt(TypeSymbol *t, ValueSymbol *s, unique_ptr<MIRValue> i)
      : type(t), symbol(s), init(std::move(i)) {}
};

struct MIRQuitStmt : MIRStmt {
  MIRQuitStmt() {}
};

struct MIRDestroyStmt : MIRStmt {
  unique_ptr<MIRValue> handlePlace = nullptr;
  MIRDestroyStmt(unique_ptr<MIRValue> h) : handlePlace(std::move(h)) {}
};
