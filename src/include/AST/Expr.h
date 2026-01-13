#pragma once

#include "ASTNode.h"
#include "Visitor.h"
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>

class ValueSymbol;
class Stmt;

using namespace std;

using StmtPtr = shared_ptr<Stmt>;
class Expr : public ASTNode {
public:
  using Ptr = shared_ptr<Expr>;
  Expr(NKind kind, Token token) : ASTNode(kind, token) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class LiteralExpr : public Expr {
public:
  string value = "null";
  LiteralExpr(Token t, const string &v)
      : Expr(NKind::LITERAL_EXPR, t), value(v) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class VarExpr : public Expr {
public:
  string name;
  VarExpr(Token t, const string &n) : Expr(NKind::VAR_EXPR, t), name(n) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  ValueSymbol *resolved;
};

class UnaryExpr : public Expr {
public:
  Token op;
  Ptr right;
  UnaryExpr(Token t, Token o, Ptr p)
      : Expr(NKind::UNARY_EXPR, t), op(o), right(p) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class BinaryExpr : public Expr {
public:
  Ptr left, right;
  Token op;
  BinaryExpr(Token t, Ptr l, Token o, Ptr r)
      : Expr(NKind::BINARY_EXPR, t), left(l), right(r), op(o) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class AssignExpr : public Expr {
public:
  Expr::Ptr target;
  Expr::Ptr value;
  Token op;

  AssignExpr(Token token, Expr::Ptr t, Token o, Expr::Ptr v)
      : Expr(NKind::ASSIGN_EXPR, token), target(t), value(v), op(o) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class MemberExpr : public Expr {
public:
  Expr::Ptr object;
  string member;
  MemberExpr(Token t, Expr::Ptr o, const std::string &m)
      : Expr(NKind::ACCESS_EXPR, t), object(std::move(o)), member(m) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  ValueSymbol *resolved;
};

class IndexExpr : public Expr {
public:
  Expr::Ptr object;
  Expr::Ptr index;

  IndexExpr(Token t, Expr::Ptr o, Expr::Ptr i)
      : Expr(NKind::INDEX_EXPR, t), object(o), index(i) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class CallExpr : public Expr {
public:
  Expr::Ptr callee;
  std::vector<Expr::Ptr> arguments;

  CallExpr(Token t, Expr::Ptr c, const std::vector<Expr::Ptr> &a)
      : Expr(NKind::CALL_EXPR, t), callee(std::move(c)),
        arguments(std::move(a)) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  ValueSymbol *resolved;
};

class TernaryExpr : public Expr {
public:
  Expr::Ptr conditon;
  Expr::Ptr then;
  Expr::Ptr else_;

  TernaryExpr(Token t, Expr::Ptr c, Expr::Ptr th, Expr::Ptr e)
      : Expr(NKind::TERNARY_EXPR, t), conditon(c), then(th), else_(e) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class NewExpr : public Expr {
public:
  std::string typeName;
  std::vector<Expr::Ptr> args;

  NewExpr(Token token, const std::string &typeName, std::vector<Expr::Ptr> args)
      : Expr(NKind::NEW_EXPR, token), typeName(typeName), args(args) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class ThisExpr : public Expr {
public:
  ThisExpr(Token token) : Expr(NKind::THIS_EXPR, token) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  ValueSymbol *resolved;
};

class SuperExpr : public Expr {
public:
  SuperExpr(Token token) : Expr(NKind::SUPER_EXPR, token) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  ValueSymbol *resolved;
};

class MatchExpr : public Expr {
public:
  class Case : public ASTNode {
  public:
    Ptr value;
    StmtPtr body;
    void accept(ASTVisitor *visitor) override { visitor->visit(this); }
    Case(Token t, Ptr v, StmtPtr b)
        : ASTNode(NKind::MATCH_CASE, t), value(v), body(b) {}
  };
  Ptr value;
  vector<shared_ptr<Case>> cases;

  MatchExpr(Token t, const string &name, Ptr v, vector<shared_ptr<Case>> c)
      : Expr(NKind::MATCH_EXPR, t), value(v), cases(c) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};