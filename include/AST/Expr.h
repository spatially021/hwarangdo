#pragma once

#include "ASTNode.h"
#include "Token.h"
#include "Visitor.h"
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

class ValueSymbol;
class TypeSymbol;
class MethodSymbol;
class EnumVariantSymbol;
class Symbol;
class Stmt;
struct HIRValue;

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
  HIRValue *hirValue = nullptr;
};

class LiteralExpr : public Expr {
public:
  string value = "null";
  LiteralExpr(Token t, const string &v)
      : Expr(NKind::LITERAL_EXPR, t), value(v) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class NameExpr : public Expr {
public:
  string name;
  NameExpr(Token t, const string &n) : Expr(NKind::VAR_EXPR, t), name(n) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }

  TypeSymbol *typeSymbol = nullptr;
  ValueSymbol *valueSymbol = nullptr;
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
  enum class OperatorType {
    ADD,
    SUB,
    MUL,
    DIV,
    REM,
    POW,

    B_AND,
    B_OR,
    B_XOR,

    AND,
    OR,

    EQ,
    NT,
    LS,  // less
    LSE, // less eq
    GR,  // greater
    GRE, // greate equal

    LSH,
    RSH,

  } op;
  Token opRaw;
  BinaryExpr(Token t, Ptr l, Token o, Ptr r)
      : Expr(NKind::BINARY_EXPR, t), left(l), right(r), opRaw(o) {
    switch (o.kind) {
    case TKind::DOUBLE_EQUAL:
      op = OperatorType::EQ;
      break;
    case TKind::BANG_EQUAL:
      op = OperatorType::NT;
      break;
    case TKind::LESS:
      op = OperatorType::LS;
      break;
    case TKind::GREATER:
      op = OperatorType::GR;
      break;
    case TKind::LESS_EQUAL:
      op = OperatorType::LSE;
      break;
    case TKind::GREATER_EQUAL:
      op = OperatorType::GRE;
      break;
    case TKind::AND:
      op = OperatorType::AND;
      break;
    case TKind::OR:
      op = OperatorType::OR;
      break;
    case TKind::DOUBLE_ANGLEBUCKET:
      op = OperatorType::LSH;
      break;
    case TKind::CARET:
      op = OperatorType::B_XOR;
      break;
    case TKind::AMPERSAND:
      op = OperatorType::B_AND;
      break;
    case TKind::PIPE:
      op = OperatorType::B_OR;
      break;
    case TKind::DOUBLE_RIGHT_ANGLE_BUCKET:
      op = OperatorType::RSH;
      break;
    case TKind::PLUS:
      op = OperatorType::ADD;
      break;
    case TKind::MINUS:
      op = OperatorType::SUB;
      break;
    case TKind::SLASH:
      op = OperatorType::DIV;
      break;
    case TKind::PERCENT:
      op = OperatorType::REM;
      break;
    case TKind::STAR:
      op = OperatorType::MUL;
      break;
    case TKind::DOUBLE_STAR:
      op = OperatorType::POW;
      break;
    default:
      throw runtime_error("[line : " + to_string(o.line) +
                          ", col : " + to_string(o.col) +
                          "] unexpected token kind in binary operator");
      break;
    }
  }
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
  ValueSymbol *resolved = nullptr;
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
  Symbol *resolved = nullptr;
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
class Range : public Expr {
public:
  ExprPtr from;
  ExprPtr to;
  ExprPtr step;
  Range(Token t, ExprPtr f, ExprPtr to_, ExprPtr s = {})
      : Expr(NKind::RANGE, t), from(std::move(f)), to(std::move(to_)),
        step(std::move(s)) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class MoveExpr : public Expr {
public:
  ExprPtr target;
  MoveExpr(Token t, ExprPtr e) : Expr(NKind::MOVE_EXPR, t), target(e) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class BorrowExpr : public Expr {
public:
  ExprPtr target;
  BorrowExpr(Token t, ExprPtr e) : Expr(NKind::BORROW_EXPR, t), target(e) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class ReferenceExpr : public Expr {
public:
  ExprPtr target;
  ReferenceExpr(Token t, ExprPtr e)
      : Expr(NKind::REFERENCE_EXPR, t), target(e) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};
