#pragma once

class LiteralExpr;
class NameExpr;
class BinaryExpr;
class UnaryExpr;
class CallExpr;
class AssignExpr;
class MemberExpr;
class ArrayAccessExpr;
class TernaryExpr;
class ThisExpr;
class SuperExpr;
class MoveExpr;
class BorrowExpr;
class ReferenceExpr;

class ExprStmt;
class VarStmt;
class BlockStmt;
class IfStmt;
class ForStmt;
class WhileStmt;
class SwitchStmt;
class Case;
class ReturnStmt;
class BreakStmt;
class ContinueStmt;
class EmptyStmt;
class DeclStmt;

class ClassDecl;
class StructDecl;
class ImplDecl;
class TraitDecl;
class EnumDecl;
class FuncDecl;
class VarDecl;
class ArrayDecl;

class TypeNode;
class ASTNode;
class TraitSig;
class Param;

class ASTVisitor {
public:
  virtual ~ASTVisitor() = default;

  // Expression visitor methods
  virtual void visit(LiteralExpr *expr) = 0;
  virtual void visit(BinaryExpr *expr) = 0;
  virtual void visit(NameExpr *expr) = 0;
  virtual void visit(UnaryExpr *expr) = 0;
  virtual void visit(CallExpr *expr) = 0;
  virtual void visit(AssignExpr *expr) = 0;
  virtual void visit(MemberExpr *expr) = 0;
  virtual void visit(ArrayAccessExpr *expr) = 0;
  virtual void visit(TernaryExpr *expr) = 0;
  virtual void visit(ThisExpr *expr) = 0;
  virtual void visit(SuperExpr *expr) = 0;
  virtual void visit(MoveExpr *expr) = 0;
  virtual void visit(BorrowExpr *expr) = 0;
  virtual void visit(ReferenceExpr *expr) = 0;

  // Statement visitor methods
  virtual void visit(ExprStmt *stmt) = 0;
  virtual void visit(BlockStmt *stmt) = 0;
  virtual void visit(IfStmt *stmt) = 0;
  virtual void visit(ForStmt *stmt) = 0;
  virtual void visit(WhileStmt *stmt) = 0;
  virtual void visit(SwitchStmt *stmt) = 0;
  virtual void visit(Case *stmt) = 0;
  virtual void visit(ReturnStmt *stmt) = 0;
  virtual void visit(BreakStmt *stmt) = 0;
  virtual void visit(ContinueStmt *stmt) = 0;
  virtual void visit(DeclStmt *stmt) = 0;
  virtual void visit(EmptyStmt *stmt) = 0;

  // declare visitor methods
  virtual void visit(ClassDecl *decl) = 0;
  virtual void visit(StructDecl *decl) = 0;
  virtual void visit(EnumDecl *decl) = 0;
  virtual void visit(ImplDecl *decl) = 0;
  virtual void visit(TraitDecl *decl) = 0;

  virtual void visit(FuncDecl *decl) = 0;
  virtual void visit(VarDecl *decl) = 0;
  virtual void visit(ArrayDecl *decl) = 0;

  virtual void visit(TypeNode *decl) = 0;
  virtual void visit(ASTNode *node) = 0;

  virtual void visit(TraitSig *sig) = 0;
  virtual void visit(Param *param) = 0;
};
