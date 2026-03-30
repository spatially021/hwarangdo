#pragma once

#include "AST/Visitor.h"
#include "string"

using std::string;

class Verifier : public ASTVisitor {
#define AST_NODE(T) void visit(T *node) override;
#include "../AST/ASTNodeList.def"
#undef AST_NODE
  void unresolved(ASTNode *node, const string &msg);
};