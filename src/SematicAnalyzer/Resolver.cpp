#pragma once

#include "../include/SemanticAnalyzer/Resolver.h"

Resolver::Resolver(SymbolTable *table):table(table){}

void Resolver::visit(LiteralExpr *expr){}
void Resolver::visit(BinaryExpr *expr){}
void Resolver::visit(VarExpr *expr){}
void Resolver::visit(UnaryExpr *expr){}
void Resolver::visit(CallExpr *expr){}
void Resolver::visit(GroupExpr *expr){}
void Resolver::visit(AssignExpr *expr){}
void Resolver::visit(AccessExpr *expr){}
void Resolver::visit(IndexExpr *expr){}
void Resolver::visit(PostfixExpr *expr){}
void Resolver::visit(ArrayAccessExpr *expr){}
void Resolver::visit(TernaryExpr *expr){}
void Resolver::visit(ThisExpr *expr){}
void Resolver::visit(SuperExpr *expr){}

// Statement Resolver::visitor methods
void Resolver::visit(ExprStmt *stmt){}
void Resolver::visit(BlockStmt *stmt){}
void Resolver::visit(IfStmt *stmt){}
void Resolver::visit(ForStmt *stmt){}
void Resolver::visit(WhileStmt *stmt){}
void Resolver::visit(SwitchStmt *stmt){}
void Resolver::visit(Case *stmt){}
void Resolver::visit(ReturnStmt *stmt){}
void Resolver::visit(BreakStmt *stmt){}
void Resolver::visit(ContinueStmt *stmt){}
void Resolver::visit(DeclStmt *stmt){}
void Resolver::visit(EmptyStmt *stmt){}

// declare Resolver::visitor methods
void Resolver::visit(ClassDecl *decl){
    
}
void Resolver::visit(StructDecl *decl){}
void Resolver::visit(EnumDecl *decl){}
void Resolver::visit(ImplDecl *decl){}
void Resolver::visit(TraitDecl *decl){}

void Resolver::visit(FuncDecl *decl){}
void Resolver::visit(VarDecl *decl){}
void Resolver::visit(ArrayDecl *decl){}

void Resolver::visit(TypeNode *decl){}
void Resolver::visit(ASTNode *node){}

void Resolver::visit(TraitSig *sig){}
void Resolver::visit(Param *param){}

void Resolver::error(const Token &token, const std::string &message) const {
  string m = "[line ";
  m += std::to_string(token.line);
  m += "] Error at '" + token.text + "': " + message;

  throw runtime_error(m);
}