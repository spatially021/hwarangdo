#pragma once

#include "AST/Decl.h"
#include "AST/Program.h"
#include "AST/Visitor.h"
#include "enums/InheritState.h"
#include "string"
#include <unordered_map>

using std::string;

struct Context {
  int loopDepth = 0;
};

class Verifier : public ASTVisitor {

public:
  Verifier(Program *p) : program(p) {}

  void verify();
  void verifyCycledInherit(ClassDecl *decl);

  unordered_map<ClassDecl *, InheritState> inheritStates;

private:
#define AST_NODE(T) void visit(T *node) override;
#include "../AST/ASTNodeList.def"
#undef AST_NODE
  void unresolved(ASTNode *node, const string &msg);

private:
  Context context;
  Program *program;
};