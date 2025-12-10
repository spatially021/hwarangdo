#pragma once

#include "../Token.h"
#include "../TokenUtil.h"
#include "ASTNode.h"
#include "Node.h"
#include "Visitor.h"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

class Expr : public ASTNode {
public:
  using Ptr = std::shared_ptr<Expr>;
  std::string evaluatedType = "null"; // ← 타입 검사 결과 저장

  Expr(NodeKind kind) : ASTNode(kind) {}
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

// ─────────────────────────────────────────────
// LiteralExpr: 리터럴 (숫자, 문자열 등)
class LiteralExpr : public Expr {
public:
  std::string value = "null";
  LiteralExpr(const std::string &v, TokKind kind)
      : Expr(NodeKind::LITERAL_EXPR), value(v) {
    if (value != "null")
      switch (kind) {
      case TokKind::INTEGER:
        this->evaluatedType = "int";
        break;
      case TokKind::FLOAT:
        this->evaluatedType = "float";
        break;
      case TokKind::BOOLEAN:
        this->evaluatedType = "boolean";
        break;
      case TokKind::STRING:
        this->evaluatedType = "string";
        break;
      case TokKind::CHAR:
        this->evaluatedType = "char";
        break;
      default:
        string message =
            "Invalid TokKind for LiteralExpr : " + tokKindToString(kind);
        throw std::runtime_error(message);
      }
  }

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

// ─────────────────────────────────────────────
// Variable Expression
class VarExpr : public Expr {
public:
  std::string name;
  VarExpr(const std::string &n) : Expr(NodeKind::VAR_EXPR), name(n) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

// ─────────────────────────────────────────────
// Binary Expression
class BinaryExpr : public Expr {
public:
  Expr::Ptr left;
  std::string op;
  Expr::Ptr right;

  BinaryExpr(Expr::Ptr left, const std::string &op, Expr::Ptr right)
      : Expr(NodeKind::BINARY_EXPR), left(left), op(op), right(right) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class UnaryExpr : public Expr {
public:
  std::string op;
  Expr::Ptr operand;
  bool isPostFix;

  UnaryExpr(const std::string &op, Expr::Ptr operand, bool fix = false)
      : Expr(NodeKind::UNARY_EXPR), op(op), operand(operand), isPostFix(fix) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class CallExpr : public Expr {
public:
  Expr::Ptr callee;            // 호출 대상
  std::vector<Expr::Ptr> args; // 인자 목록

  CallExpr(Expr::Ptr callee, const std::vector<Expr::Ptr> &args)
      : Expr(NodeKind::CALL_EXPR), callee(callee), args(args) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class GroupExpr : public Expr {
public:
  Expr::Ptr expression; // 괄호 안에 있는 단일 표현식

  GroupExpr(Expr::Ptr expr) : Expr(NodeKind::GROUP_EXPR), expression(expr) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class AssignExpr : public Expr {
public:
  Expr::Ptr target; // 변수 또는 접근식 (VarExpr, AccessExpr 등)
  std::string op;
  Expr::Ptr value;

  AssignExpr(Expr::Ptr target, std::string op, Expr::Ptr value)
      : Expr(NodeKind::ASSIGN_EXPR), target(target), op(op), value(value) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class AccessExpr : public Expr {
public:
  Expr::Ptr object; // 왼쪽 객체
  Token memberName; // 오른쪽 멤버

  AccessExpr(Expr::Ptr object, Token member)
      : Expr(NodeKind::ACCESS_EXPR), object(object), memberName(member) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class IndexExpr : public Expr {
public:
  Expr::Ptr array; // 왼쪽 피연산자
  Expr::Ptr index; // 인덱스 표현식

  IndexExpr(Expr::Ptr array, Expr::Ptr index)
      : Expr(NodeKind::INDEX_EXPR), array(array), index(index) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class PostfixExpr : public Expr {
public:
  Expr::Ptr left;
  std::string op;

  PostfixExpr(Expr::Ptr l, std::string o)
      : Expr(NodeKind::POSTFIX_EXPR), left(l), op(o) {}

  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
};

class TernaryExpr : public Expr {
public:
  Expr::Ptr conditon;
  Expr::Ptr left;
  Expr::Ptr right;
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  TernaryExpr(Expr::Ptr c, Expr::Ptr l, Expr::Ptr r)
      : Expr(NodeKind::TERNARY_EXPR), conditon(c), left(l), right(r) {}
};

class ArrayAccessExpr : public Expr {

public:
  Expr::Ptr expr;
  Expr::Ptr index;
  void accept(ASTVisitor *visitor) override { visitor->visit(this); }
  ArrayAccessExpr(Expr::Ptr e, Expr::Ptr i)
      : Expr(NodeKind::ACCESS_EXPR), expr(e), index(i) {}
};