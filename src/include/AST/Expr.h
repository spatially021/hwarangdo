#pragma once

#include "ASTNode.h"
#include "Visitor.h"
#include <memory>
#include <stdexcept>
#include <string>

using namespace std;

class Stmt;
using StmtPtr = shared_ptr<Stmt>;
class Expr : public ASTNode {
public:
  using Ptr = shared_ptr<Expr>;
  string evaluatedType = "null"; // ← 타입 검사 결과 저장

  Expr(NKind kind, Token token) : ASTNode(kind, token) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class LiteralExpr : public Expr {
  string value = "null";
  LiteralExpr(NKind k, Token t, const string &v) : Expr(k, t) {
    if (value != "null") {
      switch (t.kind) {
      case TKind::LIT_INT:
        this->evaluatedType = "int";
        break;
      case TKind::LIT_BOOL:
        evaluatedType = "bool";
        break;
      case TKind::LIT_CHARACTOR:
        evaluatedType = "char";
        break;
      case TKind::LIT_FLOAT:
        evaluatedType = "float";
        break;
      case TKind::LIT_STRING:
        evaluatedType = "string";
        break;
      default:
        string message = "Invalid TKind for literalExpr : " + t.text;
        throw runtime_error(message);
      }
    }
  }

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class VarExpr : public Expr {
  string name;
  VarExpr(Token t, const string &n) : Expr(NKind::VAR_EXPR, t), name(n) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class UnaryExpr : public Expr {
  Token op;
  Ptr right;
  UnaryExpr(Token t, Ptr p) : Expr(NKind::UNARY_EXPR, t), op(t), right(p) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class BinaryExpr : public Expr {
  Ptr left, right;
  Token op;
  BinaryExpr(Token t, Ptr l, Ptr r)
      : Expr(NKind::BINARY_EXPR, t), left(l), right(r), op(t) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class AssignExpr : public Expr {
public:
  std::string name;
  Expr::Ptr value;

  AssignExpr(Token token, const std::string &name, Expr::Ptr value)
      : Expr(NKind::ASSIGN_EXPR, token), name(name), value(value) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class MemberExpr : public Expr {
public:
  Expr::Ptr object;
  std::string memberName;

  MemberExpr(Expr::Ptr object, Token token, const std::string &name)
      : Expr(NKind::ACCESS_EXPR, token), object(object), memberName(name) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class IndexExpr : public Expr {
public:
  Expr::Ptr object;
  Expr::Ptr index;

  IndexExpr(Expr::Ptr object, Expr::Ptr index, Token token)
      : Expr(NKind::INDEX_EXPR, token), object(object), index(index) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class CallExpr : public Expr {
public:
  Expr::Ptr callee;
  std::vector<Expr::Ptr> arguments;

  CallExpr(Expr::Ptr callee, Token token, const std::vector<Expr::Ptr> &args)
      : Expr(NKind::CALL_EXPR, token), callee(callee), arguments(args) {}

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
};

class SuperExpr : public Expr {
public:
  SuperExpr(Token token) : Expr(NKind::SUPER_EXPR, token) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
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