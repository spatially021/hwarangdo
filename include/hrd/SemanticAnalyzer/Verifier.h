#pragma once

#include "hrd/AST/Decl.h"
#include "hrd/AST/Program.h"
#include "hrd/AST/Visitor.h"
#include "hrd/Recover/VerifierRecover.h"
#include "hrd/compiler/CompilerContexts.h"
#include "hrd/enums/InheritState.h"
#include "hrd/util/diagnostic/DiagnosticEngine.h"
#include "string"

using std::string;

struct Context {
  int loopDepth = 0;
};

class Verifier : public ASTVisitor {

public:
  Verifier(VerifierContext &c)
      : program(c.program), engine(c.engine), recover(*this) {}

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
  DiagnosticEngine &engine;
  VerifierRecover recover;
};