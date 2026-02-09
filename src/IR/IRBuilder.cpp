#include "IR/IRBuilder.h"

void IRBuilder::visit(LiteralExpr *expr) {}
void IRBuilder::visit(BinaryExpr *expr) {}
void IRBuilder::visit(VarExpr *expr) {}
void IRBuilder::visit(UnaryExpr *expr) {}
void IRBuilder::visit(CallExpr *expr) {}
void IRBuilder::visit(AssignExpr *expr) {}
void IRBuilder::visit(MemberExpr *expr) {}
void IRBuilder::visit(ArrayAccessExpr *expr) {}
void IRBuilder::visit(TernaryExpr *expr) {}
void IRBuilder::visit(ThisExpr *expr) {}
void IRBuilder::visit(SuperExpr *expr) {}

// Statement IRBuilder::visitor methods
void IRBuilder::visit(ExprStmt *stmt) {}
void IRBuilder::visit(BlockStmt *stmt) {}
void IRBuilder::visit(IfStmt *stmt) {}
void IRBuilder::visit(ForStmt *stmt) {}
void IRBuilder::visit(WhileStmt *stmt) {}
void IRBuilder::visit(SwitchStmt *stmt) {}
void IRBuilder::visit(Case *stmt) {}
void IRBuilder::visit(ReturnStmt *stmt) {}
void IRBuilder::visit(BreakStmt *stmt) {}
void IRBuilder::visit(ContinueStmt *stmt) {}
void IRBuilder::visit(DeclStmt *stmt) {}
void IRBuilder::visit(EmptyStmt *stmt) {}

// declare IRBuilder::visitor methods
void IRBuilder::visit(ClassDecl *decl) {}
void IRBuilder::visit(StructDecl *decl) {}
void IRBuilder::visit(EnumDecl *decl) {}
void IRBuilder::visit(ImplDecl *decl) {}
void IRBuilder::visit(TraitDecl *decl) {}
void IRBuilder::visit(TraitSig *decl) {}
void IRBuilder::visit(FuncDecl *decl) {}
void IRBuilder::visit(VarDecl *decl) {}
void IRBuilder::visit(ArrayDecl *decl) {}

void IRBuilder::visit(TypeNode *decl) {}
void IRBuilder::visit(ASTNode *node) {}
void IRBuilder::visit(Param *param) {}