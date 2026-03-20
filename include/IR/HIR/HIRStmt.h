#pragma once

#include "IR/HIR/HIRExpr.h"
#include "IR/HIR/HIRNode.h"
#include <memory>
#include <vector>
struct HIRStmt : HIRNode {
  explicit HIRStmt(HIRNodeKind k, SourceSpan s = {}) : HIRNode(k, s) {}
  virtual ~HIRStmt() = default;
};

struct HIRBlockStmt : HIRStmt {
  std::vector<std::unique_ptr<HIRStmt>> statements;

  HIRBlockStmt(SourceSpan s = {}) : HIRStmt(HIRNodeKind::BlockStmt, s) {}
};

struct HIRExprStmt : HIRStmt {
  std::unique_ptr<HIRExpr> expr;

  explicit HIRExprStmt(std::unique_ptr<HIRExpr> e, SourceSpan s = {})
      : HIRStmt(HIRNodeKind::ExprStmt, s), expr(std::move(e)) {}
};

struct HIRLocalDeclStmt : HIRStmt {
  std::unique_ptr<HIRLocal> local;
  std::unique_ptr<HIRExpr> init; // nullable

  HIRLocalDeclStmt(std::unique_ptr<HIRLocal> l, std::unique_ptr<HIRExpr> i,
                   SourceSpan s = {})
      : HIRStmt(HIRNodeKind::LocalDeclStmt, s), local(std::move(l)),
        init(std::move(i)) {}
};

struct HIRIfStmt : HIRStmt {
  std::unique_ptr<HIRExpr> condition;
  std::unique_ptr<HIRBlockStmt> thenBlock;
  std::unique_ptr<HIRBlockStmt> elseBlock; // nullable

  HIRIfStmt(std::unique_ptr<HIRExpr> cond, std::unique_ptr<HIRBlockStmt> thenB,
            std::unique_ptr<HIRBlockStmt> elseB = nullptr, SourceSpan s = {})
      : HIRStmt(HIRNodeKind::IfStmt, s), condition(std::move(cond)),
        thenBlock(std::move(thenB)), elseBlock(std::move(elseB)) {}
};

struct HIRWhileStmt : HIRStmt {
  std::unique_ptr<HIRExpr> condition;
  std::unique_ptr<HIRBlockStmt> body;

  HIRWhileStmt(std::unique_ptr<HIRExpr> cond, std::unique_ptr<HIRBlockStmt> b,
               SourceSpan s = {})
      : HIRStmt(HIRNodeKind::WhileStmt, s), condition(std::move(cond)),
        body(std::move(b)) {}
};

struct HIRForRangeStmt : HIRStmt {
  std::unique_ptr<HIRLocal> indexVar;
  std::unique_ptr<HIRExpr> start;
  std::unique_ptr<HIRExpr> end;
  std::unique_ptr<HIRExpr> step; // nullable -> default 1
  std::unique_ptr<HIRBlockStmt> body;

  HIRForRangeStmt(std::unique_ptr<HIRLocal> idx, std::unique_ptr<HIRExpr> st,
                  std::unique_ptr<HIRExpr> ed, std::unique_ptr<HIRExpr> sp,
                  std::unique_ptr<HIRBlockStmt> b, SourceSpan s = {})
      : HIRStmt(HIRNodeKind::ForRangeStmt, s), indexVar(std::move(idx)),
        start(std::move(st)), end(std::move(ed)), step(std::move(sp)),
        body(std::move(b)) {}
};

struct HIRReturnStmt : HIRStmt {
  std::unique_ptr<HIRExpr> value; // nullable

  HIRReturnStmt(std::unique_ptr<HIRExpr> v = nullptr, SourceSpan s = {})
      : HIRStmt(HIRNodeKind::ReturnStmt, s), value(std::move(v)) {}
};

struct HIRBreakStmt : HIRStmt {
  HIRBreakStmt(SourceSpan s = {}) : HIRStmt(HIRNodeKind::BreakStmt, s) {}
};

struct HIRContinueStmt : HIRStmt {
  HIRContinueStmt(SourceSpan s = {}) : HIRStmt(HIRNodeKind::ContinueStmt, s) {}
};