#pragma once

#include "hrd/IR/HIR/HIRNode.h"
#include "hrd/IR/HIR/HIRPattern.h"
#include "hrd/IR/HIR/HIRSymbol.h"
#include "hrd/IR/HIR/HIRType.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/SourceSpan.h"
#include "hrd/enums/StorageKind.h"
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

struct HIRValueExpr;
struct HIRPlaceExpr;

struct HIRStmt : HIRNode {
  explicit HIRStmt(SourceSpan s, HIRNodeKind k) : HIRNode(s, k) {}
  virtual ~HIRStmt() = default;
};

struct HIRBlockStmt : HIRStmt {
  std::vector<std::unique_ptr<HIRStmt>> statements;
  std::unordered_map<ValueSymbol *, HIRLocal *> localMap;
  HIRBlockStmt *parent = nullptr;
  HIRBlockStmt(SourceSpan s) : HIRStmt(s, HIRNodeKind::BlockStmt) {}
};

struct HIRExprStmt : HIRStmt {
  std::unique_ptr<HIRExpr> expr;

  explicit HIRExprStmt(SourceSpan s, std::unique_ptr<HIRExpr> e)
      : HIRStmt(s, HIRNodeKind::ExprStmt), expr(std::move(e)) {}
};

struct HIRLocalDeclStmt : HIRStmt {
  HIRLocal *local;
  std::unique_ptr<HIRValueExpr> init; // nullable

  HIRLocalDeclStmt(SourceSpan s, HIRLocal *l, std::unique_ptr<HIRValueExpr> i)
      : HIRStmt(s, HIRNodeKind::LocalDeclStmt), local(l), init(std::move(i)) {}
};

struct HIRIfStmt : HIRStmt {
  std::unique_ptr<HIRValueExpr> condition;
  std::unique_ptr<HIRBlockStmt> thenBlock;
  std::unique_ptr<HIRBlockStmt> elseBlock; // nullable

  HIRIfStmt(SourceSpan s, std::unique_ptr<HIRValueExpr> cond,
            std::unique_ptr<HIRBlockStmt> thenB,
            std::unique_ptr<HIRBlockStmt> elseB = nullptr)
      : HIRStmt(s, HIRNodeKind::IfStmt), condition(std::move(cond)),
        thenBlock(std::move(thenB)), elseBlock(std::move(elseB)) {}
};

struct HIRWhileStmt : HIRStmt {
  std::unique_ptr<HIRExpr> condition;
  std::unique_ptr<HIRBlockStmt> body;

  HIRWhileStmt(SourceSpan s, std::unique_ptr<HIRExpr> cond,
               std::unique_ptr<HIRBlockStmt> b)
      : HIRStmt(s, HIRNodeKind::WhileStmt), condition(std::move(cond)),
        body(std::move(b)) {}
};

struct HIRForRangeStmt : HIRStmt {
  HIRLocal *indexVar = nullptr;
  std::unique_ptr<HIRExpr> start = nullptr;
  std::unique_ptr<HIRExpr> end = nullptr;
  std::unique_ptr<HIRExpr> step = nullptr; // nullable -> default 1
  std::unique_ptr<HIRBlockStmt> body = nullptr;

  HIRForRangeStmt(SourceSpan s, HIRLocal *idx, std::unique_ptr<HIRExpr> st,
                  std::unique_ptr<HIRExpr> ed, std::unique_ptr<HIRExpr> sp,
                  std::unique_ptr<HIRBlockStmt> b)
      : HIRStmt(s, HIRNodeKind::ForRangeStmt), indexVar(std::move(idx)),
        start(std::move(st)), end(std::move(ed)), step(std::move(sp)),
        body(std::move(b)) {}
};

struct HIRReturnStmt : HIRStmt {
  std::unique_ptr<HIRExpr> value; // nullable

  HIRReturnStmt(SourceSpan s, std::unique_ptr<HIRExpr> v = nullptr)
      : HIRStmt(s, HIRNodeKind::ReturnStmt), value(std::move(v)) {}
};

struct HIRBreakStmt : HIRStmt {
  HIRBreakStmt(SourceSpan s) : HIRStmt(s, HIRNodeKind::BreakStmt) {}
};

struct HIRContinueStmt : HIRStmt {
  HIRContinueStmt(SourceSpan s) : HIRStmt(s, HIRNodeKind::ContinueStmt) {}
};

enum class HIRBranchKind {
  Match,
  Swtich,

};

struct HIRValueExpr;

enum class HIRDefaultKind {
  None,
  Default,
  WildCard,
};

struct HIRCase : HIRStmt {
  std::vector<std::unique_ptr<HIRCasePattern>>
      selectors; // literal or enum variant
  std::unique_ptr<HIRBlockStmt> body;
  HIRDefaultKind defaultKind = HIRDefaultKind::None;
  HIRCase(SourceSpan s, vector<std::unique_ptr<HIRCasePattern>> sl,
          unique_ptr<HIRBlockStmt> b, HIRDefaultKind d = HIRDefaultKind::None)
      : HIRStmt(s, HIRNodeKind::Case), selectors(std::move(sl)),
        body(std::move(b)), defaultKind(d) {}
};

struct HIRSwitchStmt : HIRStmt {
  std::unique_ptr<HIRValueExpr> cond;
  std::vector<std::unique_ptr<HIRCase>> cases;
  HIRSwitchStmt(SourceSpan s, unique_ptr<HIRValueExpr> c,
                vector<unique_ptr<HIRCase>> ca)
      : HIRStmt(s, HIRNodeKind::SwitchStmt), cond(std::move(c)),
        cases(std::move(ca)) {}
};

struct HIRValueTransferStmt : HIRStmt {
  unique_ptr<HIRValueExpr> value;
  HIRValueTransferStmt(SourceSpan s, unique_ptr<HIRValueExpr> v)
      : HIRStmt(s, HIRNodeKind::ValueTransferStmt), value(std::move(v)) {}
};

struct HIRDestroyStmt : HIRStmt {
  StorageKind storage;
  unique_ptr<HIRValueExpr> handle = nullptr;
  HIREntityType *entity = nullptr;
  HIRDestroyStmt(SourceSpan s, unique_ptr<HIRValueExpr> h, HIREntityType *e,
                 StorageKind sk)
      : HIRStmt(s, HIRNodeKind::DestroyStmt), storage(sk), handle(std::move(h)),
        entity(e) {}
};

struct HIRQuitStmt : HIRStmt {
  HIRQuitStmt(SourceSpan s) : HIRStmt(s, HIRNodeKind::QuitStmt) {}
};

struct HIRAssignStmt : HIRStmt {
  unique_ptr<HIRPlaceExpr> lhs = nullptr;
  unique_ptr<HIRValueExpr> rhs = nullptr;

  HIRAssignStmt(SourceSpan s, unique_ptr<HIRPlaceExpr> l,
                unique_ptr<HIRValueExpr> r)
      : HIRStmt(s, HIRNodeKind::AssignStmt), lhs(std::move(l)),
        rhs(std::move(r)) {}
};

struct HIRCompoundAssignStmt : HIRStmt {
  unique_ptr<HIRPlaceExpr> lhs;
  unique_ptr<HIRValueExpr> rhs;
  Operator op;

  HIRCompoundAssignStmt(SourceSpan s, unique_ptr<HIRPlaceExpr> l,
                        unique_ptr<HIRValueExpr> r, Operator o)
      : HIRStmt(s, HIRNodeKind::CompoundAssignStmt), lhs(std::move(l)),
        rhs(std::move(r)), op(o) {}
};