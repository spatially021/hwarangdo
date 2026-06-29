#pragma once

#define AST_NODE(T) class T;
#include "ASTNodeList.def"
#undef AST_NODE

class ASTVisitor {
public:
  virtual ~ASTVisitor() = default;

#define AST_NODE(T) virtual void visit(T *node) = 0;
#include "ASTNodeList.def"
#undef AST_NODE
};

class ASTVisitorBase : public ASTVisitor {
protected:
#define AST_NODE(T)                                                            \
  virtual void defaultVisit(T *) {}
#include "ASTNodeList.def"
#undef AST_NODE

public:
#define AST_NODE(T)                                                            \
  void visit(T *node) override { defaultVisit(node); }
#include "ASTNodeList.def"
#undef AST_NODE
};