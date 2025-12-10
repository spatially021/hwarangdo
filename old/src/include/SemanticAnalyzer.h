#pragma once
#include "AST/Expr.h"
#include "AST/Visitor.h"
#include "SymbolTable.h"
#include "Token.h"
#include <memory>
#include <string>

class SemanticAnalyzer : public ASTVisitor {
public:
  explicit SemanticAnalyzer(std::shared_ptr<Program>);
  private : 
  ClassSymbol *currentClass = nullptr;
    bool insideClass = false;

  public:
    std::shared_ptr<Program> program;
    SymbolTable symbols;
    std::string currentReturnType;
    // SemanticAnalyzer.h 안에 (private):
    // 또는 shared_ptr<ClassSymbol> currentClass; 를 선호하면 그에 맞게 아래
    // 코드 조정

    void analyze();

    // ────────── Expression visitor methods ──────────
    void visit(LiteralExpr *expr) override;
    void visit(BinaryExpr *expr) override;
    void visit(VarExpr *expr) override;
    void visit(UnaryExpr *expr) override;
    void visit(CallExpr *expr) override;
    void visit(GroupExpr *expr) override;
    void visit(AssignExpr *expr) override;
    void visit(AccessExpr *expr) override;
    void visit(IndexExpr *expr) override;
    void visit(PostfixExpr *expr) override;
    void visit(ArrayAccessExpr *expr) override;
    void visit(TernaryExpr *expr) override;

    // ────────── Statement visitor methods ──────────
    void visit(ExprStmt *stmt) override;
    void visit(BlockStmt *stmt) override;
    void visit(IfStmt *stmt) override;
    void visit(ForStmt *stmt) override;
    void visit(WhileStmt *stmt) override;
    void visit(SwitchStmt *stmt) override;
    void visit(CaseStmt *stmt) override;
    void visit(ReturnStmt *stmt) override;
    void visit(BreakStmt *stmt) override;
    void visit(ContinueStmt *stmt) override;
    void visit(EmptyStmt *stmt) override;

    // ────────── Declaration visitor methods ──────────
    void visit(ClassDecl *decl) override;
    void visit(StructDecl *decl) override;
    void visit(EnumDecl *decl) override;
    void visit(InterfaceDecl *decl) override;
    void visit(Program *decl) override;
    void visit(FuncDecl *decl) override;
    void visit(VarDecl *decl, bool isInFor) override;
    void visit(ArrayDecl *decl) override;
    void visit(TypeNode *decl) override;
    void visit(ASTNode *node) override;

  private:
    // 현재 분석 중인 함수들 스택
    std::vector<FuncSymbol *> currentFunctionStack;

    int canBreak = 0;
    int canContinue = 0;
};
