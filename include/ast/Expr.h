#pragma once
#include "ASTNode.h"
#include "Visitor.h"
#include <string>
#include <memory>

class Expr : public ASTNode {
public:
    using Ptr = std::shared_ptr<Expr>;
    Expr(NodeKind kind) : ASTNode(kind) {}
};

// ─────────────────────────────────────────────
// LiteralExpr: 리터럴 (숫자, 문자열 등)
class LiteralExpr : public Expr {
public:
    std::string value;
    LiteralExpr(const std::string& v)
        : Expr(NodeKind::LITERAL_EXPR), value(v) {}

    void accept(ASTVisitor* visitor) override {
        visitor->visit(this);
    }
};

// ─────────────────────────────────────────────
// Variable Expression
class VarExpr : public Expr {
public:
    std::string name;
    VarExpr(const std::string& n)
        : Expr(NodeKind::VAR_EXPR), name(n) {}

    void accept(ASTVisitor* visitor) override {
        visitor->visit(this);
    }
};

// ─────────────────────────────────────────────
// Binary Expression
class BinaryExpr : public Expr {
public:
    Expr::Ptr left;
    std::string op;
    Expr::Ptr right;

    BinaryExpr(Expr::Ptr left, const std::string& op, Expr::Ptr right)
        : Expr(NodeKind::BINARY_EXPR), left(left), op(op), right(right) {}

    void accept(ASTVisitor* visitor) override {
        visitor->visit(this);
    }
};

class UnaryExpr : public Expr{
public:
    Expr::Ptr left;
    std::string op;
    Expr::Ptr right;


    UnaryExpr(Expr::Ptr left,const std::string& op, Expr::Ptr right)
        : Expr(NodeKind::UNARY_EXPR), left(left), op(op) right(right){}

    void accept(ASTVisitor* visitor) override {
        visitor->visit(this);
    }

}

class CallExpr : public Expr{
public:
    Expr::Ptr left;
    std::string op;
    Expr::Ptr right;


    CallExpr(Expr::Ptr left,const std::string& op, Expr::Ptr right)
        : Expr(NodeKind::CALL_EXPR), left(left), op(op) right(right){}

    void accept(ASTVisitor* visitor) override {
        visitor->visit(this);
    }

}

class GroupExpr : public Expr{
public:
    Expr::Ptr left;
    std::string op;
    Expr::Ptr right;


    GroupExpr(Expr::Ptr left,const std::string& op, Expr::Ptr right)
        : Expr(NodeKind::GROUP_EXPR), left(left), op(op) right(right){}

    void accept(ASTVisitor* visitor) override {
        visitor->visit(this);
    }

}

class AssignExpr : public Expr{
public:
    Expr::Ptr left;
    std::string op;
    Expr::Ptr right;


    AssignExpr(Expr::Ptr left,const std::string& op, Expr::Ptr right)
        : Expr(NodeKind::ASSIGN_EXPR), left(left), op(op) right(right){}

    void accept(ASTVisitor* visitor) override {
        visitor->visit(this);
    }

}

class AccessExpr : public Expr{
public:
    Expr::Ptr left;
    std::string op;
    Expr::Ptr right;


    AccessExpr(Expr::Ptr left,const std::string& op, Expr::Ptr right)
        : Expr(NodeKind::ACCESS_EXPR), left(left), op(op) right(right){}

    void accept(ASTVisitor* visitor) override {
        visitor->visit(this);
    }

}

class IndexExpr : public Expr{
public:
    Expr::Ptr left;
    std::string op;
    Expr::Ptr right;


    IndexExpr(Expr::Ptr left,const std::string& op, Expr::Ptr right)
        : Expr(NodeKind::INDEX_EXPR), left(left), op(op) right(right){}

    void accept(ASTVisitor* visitor) override {
        visitor->visit(this);
    }

}