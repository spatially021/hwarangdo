#include "Debugger/ParserDebugger.h"
#include "AST/ASTNode.h"
#include "AST/Decl.h"
#include "AST/Stmt.h"
#include <iostream>
#include <string>

using std::cout;
using std::string;

string ParserDebugger::ident() { return string(depth * 2, ' '); }

void ParserDebugger::visit(LiteralExpr *expr) { cout << " " << expr->value; }
void ParserDebugger::visit(BinaryExpr *expr) {
  cout << " ";
  expr->left->accept(this);
  cout << " " << expr->op.text;
  expr->right->accept(this);
}
void ParserDebugger::visit(VarExpr *expr) { cout << " " << expr->name; }
void ParserDebugger::visit(UnaryExpr *expr) {
  cout << " " << expr->op.text;
  expr->right->accept(this);
}
void ParserDebugger::visit(CallExpr *expr) {
  cout << " ";
  expr->receiver->accept(this);
  cout << " " << expr->methodName << "(";
  for (auto p : expr->arguments) {
    p->accept(this);
    cout << " ";
  }
  cout << ")";
}
void ParserDebugger::visit(AssignExpr *expr) {
  expr->target->accept(this);
  cout << " " << expr->op.text;
  expr->value->accept(this);
}
void ParserDebugger::visit(MemberExpr *expr) {
  expr->object->accept(this);
  cout << "." << expr->member;
}
void ParserDebugger::visit(ArrayAccessExpr *expr) {
  expr->object->accept(this);
  cout << "[ ";
  expr->index->accept(this);
  cout << " ]";
}
void ParserDebugger::visit(TernaryExpr *expr) {
  expr->conditon->accept(this);
  cout << " ? ";
  expr->then->accept(this);
  cout << " : ";
  expr->else_->accept(this);
}
void ParserDebugger::visit(ThisExpr *) { cout << "this"; }
void ParserDebugger::visit(SuperExpr *) { cout << "super"; }

void ParserDebugger::visit(ExprStmt *stmt) {
  cout << ident();
  stmt->expr->accept(this);
  cout << "\n";
}
void ParserDebugger::visit(BlockStmt *stmt) {
  depth++;
  for (auto s : stmt->statements)
    s->accept(this);
  depth--;
}
void ParserDebugger::visit(IfStmt *stmt) {
  cout << ident() << "if(";
  stmt->condition->accept(this);
  cout << ")";
  stmt->thenBranch->accept(this);
  if (stmt->elseBranch != nullptr)
    stmt->elseBranch->accept(this);
}
void ParserDebugger::visit(ForStmt *stmt) {
  cout << ident() << "for(";
  stmt->initializer->accept(this);
  stmt->range->accept(this);
  cout << ")";
  stmt->body->accept(this);
}
void ParserDebugger::visit(WhileStmt *stmt) {
  cout << ident() << "while(";
  stmt->condition->accept(this);
  cout << ")";
  stmt->body->accept(this);
}
void ParserDebugger::visit(SwitchStmt *stmt) {
  cout << ident() << "switch(";
  stmt->value->accept(this);
  cout << ")\n";
  depth++;
  for (auto c : stmt->clauses)
    c->accept(this);
  depth--;
}
void ParserDebugger::visit(Case *stmt) {
  cout << ident() << "case ";
  for (auto v : stmt->values)
    v->accept(this);
  stmt->body->accept(this);
}
void ParserDebugger::visit(ReturnStmt *stmt) {
  cout << ident() << "return";
  stmt->value->accept(this);
  cout << "\n";
}
void ParserDebugger::visit(BreakStmt *) { cout << ident() << "break"; }
void ParserDebugger::visit(ContinueStmt *) { cout << ident() << "cotinue"; }
void ParserDebugger::visit(DeclStmt *stmt) { stmt->decl->accept(this); }
void ParserDebugger::visit(EmptyStmt *) {}

// declare ParserDebugger::visitor methods
void ParserDebugger::visit(ClassDecl *decl) {
  cout << ident() << "[classDecl] name : " << decl->name << " , baseClass : "
       << (decl->baseClass.has_value() ? decl->baseClass.value() : " ")
       << " , implements : ";
  for (auto s : decl->traits)
    cout << s << " ";
  cout << "\n";
  depth++;
  for (auto b : decl->body)
    b->accept(this);
  depth--;
}
void ParserDebugger::visit(StructDecl *decl) {
  cout << ident() << "[structDecl] name : " << decl->name << "\n";
  depth++;
  for (auto f : decl->fields)
    f->accept(this);
  depth--;
}
void ParserDebugger::visit(EnumDecl *decl) {
  cout << ident() << "[enumDecl] name : " << decl->name << "\n";
  depth++;
  for (auto v : decl->variants) {
    cout << ident() << v->name << "\n";
  }
  depth--;
}
void ParserDebugger::visit(ImplDecl *decl) {
  cout << ident() << "[implDecl] target : " << decl->target << "\n";
  depth++;
  for (auto m : decl->LinkedImplMethods)
    m->accept(this);
  depth--;
}
void ParserDebugger::visit(TraitDecl *decl) {
  cout << ident() << "[traitDecl] name : " << decl->name << "\n";
  depth++;
  for (auto t : decl->traitSigs)
    t->accept(this);
  depth--;
}
void ParserDebugger::visit(TraitSig *decl) {
  cout << ident() << decl->name << "(";
  for (auto p : decl->params)
    p->accept(this);
  cout << ")\n";
}
void ParserDebugger::visit(FuncDecl *decl) {
  cout << ident() << "[funcDecl] " << decl->name << "(";
  for (auto p : decl->params)
    p->accept(this);
  cout << ")\n";
  depth++;
  decl->body->accept(this);
  depth--;
}
void ParserDebugger::visit(VarDecl *decl) {
  cout << ident() << "[varDecl] ";
  decl->type->accept(this);
  cout << " " << decl->name;
  if (decl->init != nullptr) {
    cout << " = ";
    decl->init.value()->accept(this);
  }
  cout << "\n";
}
void ParserDebugger::visit(ArrayDecl *decl) {
  cout << ident() << "[arrayDecl] ";
  decl->type->accept(this);
  cout << decl->name;
  if (decl->init.has_value()) {
    cout << " = ";
    decl->init.value()->accept(this);
  }
  cout << "\n";
}

void ParserDebugger::visit(TypeNode *decl) { cout << decl->type; }
void ParserDebugger::visit(ASTNode *) {}
void ParserDebugger::visit(Param *param) {
  cout<<" ";
  param->type->accept(this);
  cout << " " << param->name;
  if (param->defaultValue.has_value()) {
    cout << " = ";
    param->defaultValue.value()->accept(this);
  }
}