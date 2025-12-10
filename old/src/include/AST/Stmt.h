#pragma once
#include "ASTNode.h"
#include "Expr.h"
#include "Visitor.h"
#include <memory>
#include <string>
#include <variant>
#include <vector>

class Stmt : public ASTNode {
public:
  using Ptr = std::shared_ptr<Stmt>;
  Stmt(NodeKind kind) : ASTNode(kind) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

// ────────────────────────────────
// Expression Statement (EXPR_STMT)
class ExprStmt : public Stmt {
public:
  Expr::Ptr expr;
  ExprStmt(Expr::Ptr e) : Stmt(NodeKind::EXPR_STMT), expr(e) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

// ────────────────────────────────

// ────────────────────────────────
// Block Statement (BLOCK_STMT)
class BlockStmt : public Stmt {
public:
  std::vector<Stmt::Ptr> statements;

  BlockStmt(const std::vector<Stmt::Ptr> &stmts = {})
      : Stmt(NodeKind::BLOCK_STMT), statements(stmts) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

// ────────────────────────────────
// If Statement (IF_STMT)
class IfStmt : public Stmt {
public:
  Expr::Ptr condition;
  Stmt::Ptr thenBranch;
  Stmt::Ptr elseBranch;

  IfStmt(Expr::Ptr cond, Stmt::Ptr thenB, Stmt::Ptr elseB = nullptr)
      : Stmt(NodeKind::IF_STMT), condition(cond), thenBranch(thenB),
        elseBranch(elseB) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

// ────────────────────────────────
// For Statement (FOR_STMT)
class ForStmt : public Stmt {
public:
  std::variant<Stmt::Ptr, Expr::Ptr, std::nullptr_t> initializer;
  Expr::Ptr condition;
  Expr::Ptr increment;
  Stmt::Ptr body;

  ForStmt(std::variant<Stmt::Ptr, Expr::Ptr, std::nullptr_t> init,
          Expr::Ptr cond, Expr::Ptr inc, Stmt::Ptr b)
      : Stmt(NodeKind::FOR_STMT), initializer(init), condition(cond),
        increment(inc), body(b) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

// ────────────────────────────────
// While Statement (WHILE_STMT)
class WhileStmt : public Stmt {
public:
  Expr::Ptr condition;
  Stmt::Ptr body;

  WhileStmt(Expr::Ptr cond, Stmt::Ptr b)
      : Stmt(NodeKind::WHILE_STMT), condition(cond), body(b) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class CaseStmt : public Stmt {
public:
  Expr::Ptr value;             // nullptr이면 default case
  std::vector<Stmt::Ptr> body; // 여러 문장 포함

  CaseStmt(Expr::Ptr val, const std::vector<Stmt::Ptr> &stmts)
      : Stmt(NodeKind::CASE_STMT), value(val), body(stmts) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class SwitchStmt : public Stmt {
public:
  Expr::Ptr expression;
  std::vector<std::shared_ptr<CaseStmt>> cases;

  SwitchStmt(Expr::Ptr expr, const std::vector<std::shared_ptr<CaseStmt>> &c)
      : Stmt(NodeKind::SWITCH_STMT), expression(expr), cases(c) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

// ────────────────────────────────
// Return Statement (RETURN_STMT)
class ReturnStmt : public Stmt {
public:
  Expr::Ptr value;

  ReturnStmt(Expr::Ptr val = nullptr)
      : Stmt(NodeKind::RETURN_STMT), value(val) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

// ────────────────────────────────
// Break Statement (BREAK_STMT)
class BreakStmt : public Stmt {
public:
  BreakStmt() : Stmt(NodeKind::BREAK_STMT) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

// ────────────────────────────────
// Continue Statement (CONTINUE_STMT)
class ContinueStmt : public Stmt {
public:
  ContinueStmt() : Stmt(NodeKind::CONTINUE_STMT) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

// ────────────────────────────────
// Empty Statement (EMPTY_STMT)
class EmptyStmt : public Stmt {
public:
  EmptyStmt() : Stmt(NodeKind::EMPTY_STMT) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};
