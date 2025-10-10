#pragma once
#include "ASTNode.h"
#include "Expr.h"
#include "Visitor.h"
#include <vector>
#include <memory>

class Stmt : public ASTNode {
public:
    using Ptr = std::shared_ptr<Stmt>;
    Stmt(NodeKind kind) : ASTNode(kind) {}
};

// ─────────────────────────────────────────────
// Expression Statement
class ExprStmt : public Stmt {
public:
    Expr::Ptr expr;
    ExprStmt(Expr::Ptr expr)
        : Stmt(NodeKind::EXPR_STMT), expr(expr) {}

    void accept(ASTVisitor* visitor) override {
        visitor->visit(this);
    }
};

// ─────────────────────────────────────────────
// Variable Declaration
class VarStmt : public Stmt {
public:
    std::string name;
    Expr::Ptr initializer;

    VarStmt(const std::string& name, Expr::Ptr init)
        : Stmt(NodeKind::VAR_STMT), name(name), initializer(init) {}

    void accept(ASTVisitor* visitor) override {
        visitor->visit(this);
    }
};

// ─────────────────────────────────────────────
// Block Statement
class BlockStmt : public Stmt {
public:
    std::vector<Stmt::Ptr> statements;

    BlockStmt(const std::vector<Stmt::Ptr>& stmts)
        : Stmt(NodeKind::BLOCK_STMT), statements(stmts) {}

    void accept(ASTVisitor* visitor) override {
        visitor->visit(this);
    }
};

// ─────────────────────────────────────────────
// If Statement
class IfStmt : public Stmt {
public:
    Expr::Ptr condition;
    Stmt::Ptr thenBranch;
    Stmt::Ptr elseBranch;

    IfStmt(Expr::Ptr cond, Stmt::Ptr thenB, Stmt::Ptr elseB = nullptr)
        : Stmt(NodeKind::IF_STMT), condition(cond),
          thenBranch(thenB), elseBranch(elseB) {}

    void accept(ASTVisitor* visitor) override {
        visitor->visit(this);
    }
};