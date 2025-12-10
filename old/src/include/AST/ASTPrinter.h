#pragma once

#include "Decl.h"
#include "Expr.h"
#include "Stmt.h"
#include "Visitor.h"
#include <iostream>

using namespace std;

class PrintVisitor : public ASTVisitor {
  int depth = 0;

public:
  // ────── Expressions ──────
  void visit(LiteralExpr *expr) override { std::cout << expr->value; }

  void visit(VarExpr *expr) override { std::cout << expr->name; }

  void visit(BinaryExpr *expr) override {

    expr->left->accept(this);
    std::cout << " " << expr->op << " ";
    expr->right->accept(this);
  }

  void visit(UnaryExpr *expr) override {
    cout << expr->op;
    expr->operand->accept(this);
  }

  void visit(CallExpr *expr) override {
    expr->callee->accept(this);
    cout << "(";
    unsigned int i = 0;
    for (auto const &a : expr->args) {
      a->accept(this);
      cout << " ";
      if (++i != expr->args.size())
        cout << ",";
    }
    cout << ")";
  }

  void visit(ArrayAccessExpr *expr) override {
    expr->expr->accept(this);
    cout << "[";
    expr->index->accept(this);
    cout << "]";
  }

  void visit(GroupExpr *expr) override {
    std::cout << "(";
    expr->expression->accept(this);
    std::cout << ")";
  }

  void visit(AssignExpr *expr) override {
    expr->target->accept(this);
    cout << " " << expr->op << " ";
    expr->value->accept(this);
  }

  void visit(TernaryExpr *expr) override {
    expr->conditon->accept(this);
    cout << ":";
    expr->left->accept(this);
    cout << "?";
    expr->right->accept(this);
  }

  void visit(AccessExpr *expr) override {
    expr->object->accept(this);
    cout << "." << (expr->memberName.text);
  }

  void visit(IndexExpr *expr) override { cout << "index"; }

  void visit(PostfixExpr *expr) override {
    expr->left->accept(this);
    cout << expr->op;
  }
  // ────── Statements ──────
  void visit(ExprStmt *stmt) override {
    tab();
    cout << "[expression] ";
    stmt->expr->accept(this);
    cout << "\n";
  }

  void visit(BlockStmt *stmt) override {
    tab();
    cout << "\n";
    for (auto &s : stmt->statements)
      s->accept(this);
  }

  void visit(IfStmt *stmt) override {
    tab();
    cout << MAGENTA << "[if] condition - ";
    stmt->condition->accept(this);
    depth++;
    cout << RESET;
    stmt->thenBranch->accept(this);
    depth--;
    if (stmt->elseBranch != nullptr) {
      tab();
      cout << MAGENTA << "[else]" << RESET;
      depth++;
      stmt->elseBranch->accept(this);
      depth--;
    }
  }

  void visit(ForStmt *stmt) override {
    tab();
    std::cout << AMBER << "[for] (";

    // --- 초기식 ---
    if (stmt->initializer.index() == 0) {
      auto initStmt = get<0>(stmt->initializer);
      if (initStmt) {
        // VarDecl 형태 → for 문 내부 전용 출력
        visit(dynamic_cast<VarDecl *>(initStmt.get()), /*inFor=*/true);
      }
    } else if (stmt->initializer.index() == 1) {
      auto initExpr = get<1>(stmt->initializer);
      if (initExpr)
        initExpr->accept(this);
    }
    std::cout << "; ";

    // --- 조건식 ---
    if (stmt->condition)
      stmt->condition->accept(this);
    std::cout << "; ";

    // --- 증가식 ---
    if (stmt->increment)
      stmt->increment->accept(this);
    std::cout << ") " << RESET;

    // --- 본문 ---
    depth++;
    if (stmt->body)
      stmt->body->accept(this);
    depth--;
  }

  void visit(WhileStmt *stmt) override {
    tab();
    cout << AMBER << "[while] condition - ";
    stmt->condition->accept(this);
    cout << RESET;
    depth++;
    stmt->body->accept(this);
    depth--;
  }

  void visit(SwitchStmt *stmt) override {
    tab();
    std::cout << MAGENTA << "[switch] value - ";
    stmt->expression->accept(this);
    cout << "\n" << RESET;
    depth++;
    for (auto &c : stmt->cases)
      c->accept(this);
    depth--;
  }

  void visit(CaseStmt *stmt) override {
    tab();
    cout << MAGENTA;
    if (stmt->value) {
      std::cout << "[case] ";
      stmt->value->accept(this);
    } else {
      std::cout << "[default]";
    }
    std::cout << "\n" << RESET;

    depth++;
    for (const auto &s : stmt->body) {
      if (s)
        s->accept(this);
    }
    depth--;
  }

  void visit(ReturnStmt *stmt) override {
    tab();
    std::cout << "[return]";
    if (stmt->value) {
      std::cout << " ";
      stmt->value->accept(this);
    }
    cout << "\n";
  }

  void visit(BreakStmt *stmt) override {
    tab();
    std::cout << "[break]" << std::endl;
  }

  void visit(ContinueStmt *stmt) override {
    tab();
    std::cout << "[continue]" << std::endl;
  }

  void visit(EmptyStmt *stmt) override {}

  // ────── Declarations ──────
  void visit(Program *program) override {
    for (auto &stmt : program->declarations)
      stmt->accept(this);
  }

  void visit(ClassDecl *decl) override {
    tab();
    cout << BLUE << "[class]" << "name - " << decl->name << RESET << "\n";
    depth++;
    for (const auto &m : decl->members) {
      if (m != nullptr)
        m->accept(this);
    }
    depth--;
  }

  void visit(StructDecl *decl) override {
    std::cout << "struct " << decl->name << " {\n";
    for (auto member : decl->members)
      member->accept(this);
    std::cout << "}\n";
  }

  void visit(EnumDecl *decl) override {
    std::cout << "enum " << decl->name << " { ";
    for (auto &val : decl->values)
      std::cout << val << " ";
    std::cout << "}\n";
  }

  void visit(InterfaceDecl *decl) override {
    std::cout << "interface " << decl->name << " {\n";
    for (auto method : decl->methods)
      method->accept(this);
    std::cout << "}\n";
  }

  void visit(VarDecl *decl, bool inFor) override {
    if (inFor) {
      cout << decl->type->type << " " << decl->name << " = ";
      decl->init->accept(this);
    } else {
      tab();
      cout << YELLOW << "[variation-declaration] " << decl->type->type << " "
           << decl->name << " = ";
      decl->init->accept(this);
      cout << RESET << "\n";
    }
  }

  void visit(ArrayDecl *decl) override {
    tab();
    cout << YELLOW;
    cout << "[array-declation] " << decl->type->type << " " << decl->name
         << "[";
    decl->index->accept(this);
    cout << "]\n" << RESET;
  }

  void visit(FuncDecl *decl) override {
    tab();
    cout << CYAN << "[function-declaration] name - ";
    cout << decl->name << " returnType - " << decl->returnType->type
         << ", parameters - {";
    unsigned int i = 0;
    for (auto const &p : decl->params) {
      cout << "( name : " << p->name << ", type : " << p->type->type
           << ", DefaultValue : " << p->value.value << ")";
      if (decl->params.size() != ++i)
        cout << ", ";
    }
    cout << "}" << RESET << "\n";
    depth++;
    for (auto const &p : decl->members) {
      p->accept(this);
    }
    depth--;
  }

  void visit(TypeNode *type) override { std::cout << type->type << "\n"; }

  void visit(ASTNode *node) override {}

private:
  void tab() {
    for (int i = 0; i < depth; i++)
      cout << "\t";
  }
};
