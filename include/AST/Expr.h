#pragma once

#include "ASTNode.h"
#include "Visitor.h"
#include <memory>
#include <string>
#include <utility>

class ValueSymbol;
class TypeSymbol;
class MethodSymbol;
class EnumVariantSymbol;
class Symbol;
class Stmt;

using namespace std;

using StmtPtr = shared_ptr<Stmt>;
class Expr : public ASTNode {
public:
  using Ptr = shared_ptr<Expr>;
  Expr(NKind k, Token t) : ASTNode(k, t) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  TypeSymbol *resolvedType = nullptr;
  enum class State {
    RESOLVED,
    NEED_CHECK,
    UNKNOWN,
  } state = Expr::State::RESOLVED;
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
  ValueSymbol *resolved = nullptr;
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

  AssignExpr(Token tok, Expr::Ptr t, Token o, Expr::Ptr v)
      : Expr(NKind::ASSIGN_EXPR, tok), target(t), value(v), op(o) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class MemberExpr : public Expr {
public:
  Expr::Ptr object;
  string member;
  MemberExpr(Token t, Expr::Ptr o, const std::string &m)
      : Expr(NKind::MEMBER_EXPR, t), object(std::move(o)), member(m) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  ValueSymbol *valeuResolved = nullptr;
  EnumVariantSymbol *variantResolved = nullptr;
};

class ArrayAccessExpr : public Expr {
public:
  Expr::Ptr object;
  Expr::Ptr index;

  ArrayAccessExpr(Token t, Expr::Ptr o, Expr::Ptr i)
      : Expr(NKind::ARRAY_ACCESS_EXPR, t), object(o), index(i) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class CallExpr : public Expr {
public:
  Expr::Ptr receiver;
  string methodName;
  std::vector<Expr::Ptr> arguments;

  enum class CallType {
    FUNC_CALL,
    PAYLOAD_CALL,
    UNRESOLVED,
  } callType = CallExpr::CallType::UNRESOLVED;

  CallExpr(Token t, Expr::Ptr r, const string n,
           const std::vector<Expr::Ptr> &a)
      : Expr(NKind::CALL_EXPR, t), receiver(std::move(r)), methodName(n),
        arguments(std::move(a)) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  MethodSymbol *methodResolved = nullptr;
  EnumVariantSymbol *VariantResolved = nullptr;
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

  NewExpr(Token t, const std::string &ty, std::vector<Expr::Ptr> a)
      : Expr(NKind::NEW_EXPR, t), typeName(ty), args(a) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class ThisExpr : public Expr {
public:
  ThisExpr(Token t) : Expr(NKind::THIS_EXPR, t) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  TypeSymbol *resolved = nullptr;
};

class SuperExpr : public Expr {
public:
  SuperExpr(Token t) : Expr(NKind::SUPER_EXPR, t) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  TypeSymbol *resolved = nullptr;
};

class MatchExpr : public Expr {
public:
  class Case : public ASTNode, public enable_shared_from_this<Case> {
  public:
    Ptr value;
    StmtPtr body;
    void accept(ASTVisitor *visitor) override { visitor->visit(this); }
    Case(Token t, Ptr v, StmtPtr b)
        : ASTNode(NKind::MATCH_CASE, t), value(v), body(b) {}
  };
  Ptr value;
  vector<shared_ptr<Case>> cases;

  MatchExpr(Token t, Ptr v, vector<shared_ptr<Case>> c)
      : Expr(NKind::MATCH_EXPR, t), value(v), cases(c) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class EnumVariantExpr : public Expr {
public:
  string name;
  Expr::Ptr receiver;
  Expr::Ptr payload;
  EnumVariantExpr(Token t, string const &n, Expr::Ptr r, Expr::Ptr p)
      : Expr(NKind::ENUM_VARIANT_EXPR, t), name(n), receiver(std::move(r)),
        payload(std::move(p)) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};