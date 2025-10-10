#pragma once
#include <memory>

// Forward declarations
class LiteralExpr;
class BinaryExpr;
class VarExpr;
class ExprStmt;
class VarStmt;
class BlockStmt;
class IfStmt;

class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;

    // Expression visitor methods
    virtual void visit(LiteralExpr* expr) = 0;
    virtual void visit(BinaryExpr* expr) = 0;
    virtual void visit(VarExpr* expr) = 0;

    // Statement visitor methods
    virtual void visit(ExprStmt* stmt) = 0;
    virtual void visit(VarStmt* stmt) = 0;
    virtual void visit(BlockStmt* stmt) = 0;
    virtual void visit(IfStmt* stmt) = 0;
};
