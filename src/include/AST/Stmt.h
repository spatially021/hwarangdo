#pragma once

#include "ASTNode.h"
#include "Expr.h"
#include "Visitor.h"
#include <algorithm>
#include <memory>
#include <vector>

class Decl;
using DeclPtr = shared_ptr<Decl>;

class Stmt : public ASTNode {
public:
  using Ptr = std::shared_ptr<Stmt>;
  Stmt(NKind kind, Token token) : ASTNode(kind, token) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class ExprStmt : public Stmt {
public:
  Expr::Ptr expr;

  ExprStmt(Token token, Expr::Ptr e)
      : Stmt(NKind::EXPR_STMT, token), expr(std::move(e)) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class BlockStmt : public Stmt {
public:
  std::vector<Stmt::Ptr> statements;

  BlockStmt(Token token, std::vector<Stmt::Ptr> stmts)
      : Stmt(NKind::BLOCK_STMT, token), statements(std::move(stmts)) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class IfStmt : public Stmt {
public:
  Expr::Ptr condition;
  Stmt::Ptr thenBranch;
  Stmt::Ptr elseBranch;

  IfStmt(Token token, Expr::Ptr cond, Stmt::Ptr thenB,
         Stmt::Ptr elseB = nullptr)
      : Stmt(NKind::IF_STMT, token), condition(std::move(cond)),
        thenBranch(std::move(thenB)), elseBranch(std::move(elseB)) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class WhileStmt : public Stmt {
public:
  Expr::Ptr condition;
  Stmt::Ptr body;

  WhileStmt(Token token, Expr::Ptr c, Stmt::Ptr b)
      : Stmt(NKind::WHILE_STMT, token), condition(std::move(c)),
        body(std::move(b)) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class ForStmt : public Stmt {
public:
  Stmt::Ptr initializer; // VarDeclStmt or ExprStmt or null
  Expr::Ptr condition;
  Expr::Ptr increment;
  Stmt::Ptr body;

  ForStmt(Token token, Stmt::Ptr init, Expr::Ptr cond, Expr::Ptr inc,
          Stmt::Ptr b)
      : Stmt(NKind::FOR_STMT, token), initializer(std::move(init)),
        condition(std::move(cond)), increment(std::move(inc)),
        body(std::move(b)) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class ReturnStmt : public Stmt {
public:
  Expr::Ptr value; // null이면 return;

  ReturnStmt(Token token, Expr::Ptr v)
      : Stmt(NKind::RETURN_STMT, token), value(std::move(v)) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class VarDeclStmt : public Stmt {
public:
  std::string name;
  std::shared_ptr<TypeNode> type;
  Expr::Ptr initializer;

  VarDeclStmt(Token token, std::string n, std::shared_ptr<TypeNode> t,
              Expr::Ptr init)
      : Stmt(NKind::DECL_STMT, token), name(std::move(n)), type(std::move(t)),
        initializer(std::move(init)) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class BreakStmt : public Stmt {
public:
  BreakStmt(Token token) : Stmt(NKind::BREAK_STMT, token) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class ContinueStmt : public Stmt {
public:
  ContinueStmt(Token token) : Stmt(NKind::CONTINUE_STMT, token) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class SwitchStmt : public Stmt {
public:
  class Case : public ASTNode {
  public:
    vector<Expr::Ptr> values;
    Ptr body;
    bool isDefault;
    void accept(ASTVisitor *visitor) override { visitor->visit(this); }
    Case(Token t, vector<Expr::Ptr> v, Ptr b, bool is = false)
        : ASTNode(NKind::SWITCH_CASE, t), body(b), values(std::move(v)), isDefault(is) {}
  };
  Expr::Ptr value;                // switch (value)
  std::vector<shared_ptr<Case>> clauses; // CaseStmt 또는 DefaultStmt 의 집합

  SwitchStmt(Token token, Expr::Ptr val, std::vector<shared_ptr<Case>> c)
      : Stmt(NKind::SWITCH_STMT, token), value(std::move(val)),
        clauses(std::move(c)) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class CatchClause : public ASTNode {
public:
  std::string exceptionName;      // catch (e)
  std::shared_ptr<TypeNode> type; // catch (e: ErrorType)
  Stmt::Ptr body;                 // block or single stmt

  CatchClause(Token token, std::string name, std::shared_ptr<TypeNode> t,
              Stmt::Ptr b)
      : ASTNode(NKind::CATCH_STMT, token), exceptionName(std::move(name)),
        type(std::move(t)), body(std::move(b)) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class TryCatchStmt : public Stmt {
public:
  Stmt::Ptr tryBlock; // usually BlockStmt
  std::vector<std::shared_ptr<CatchClause>> catches;

  TryCatchStmt(Token token, Stmt::Ptr tryB,
               std::vector<std::shared_ptr<CatchClause>> c)
      : Stmt(NKind::TRY_STMT, token), tryBlock(std::move(tryB)),
        catches(std::move(c)) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class OnExitStmt : public Stmt {
public:
  Stmt::Ptr body;

  OnExitStmt(Token token, Stmt::Ptr body)
      : Stmt(NKind::ONEXIT_STMT, token), body(std::move(body)) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class EmptyStmt : public Stmt {
public:
  EmptyStmt(Token t) : Stmt(NKind::EMPTY_STMT, t) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class DeclStmt : public Stmt {
public:
  DeclPtr decl;
  DeclStmt(Token t, DeclPtr d) : Stmt(NKind::DECL_STMT, t), decl(d) {}
};