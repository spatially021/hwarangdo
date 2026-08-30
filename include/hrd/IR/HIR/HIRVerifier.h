
#include "hrd/IR/HIR/HIRDecl.h"
#include "hrd/IR/HIR/HIRExpr.h"
#include "hrd/IR/HIR/HIRPattern.h"
#include "hrd/IR/HIR/HIRProgram.h"
#include "hrd/IR/HIR/HIRStmt.h"
#include "hrd/IR/HIR/HIRSymbol.h"
#include "hrd/IR/HIR/VerifierStructs.h"
#include "hrd/Recover/HIRVerifierRecover.h"
#include "hrd/compiler/CompilerContexts.h"
#include "hrd/diagnostic/DiagnosticEngine.h"
#include "hrd/enums/InheritState.h"

#include <magic_enum/magic_enum.hpp>

class HIRVerifier {

private:
  unordered_map<HIRTypeDecl *, InheritState> inheritStates;

public:
  HIRProgram *program = nullptr;
  DiagnosticEngine &engine;
  HIRVerifierRecover recover;
  HIRVerifier(HIRVerifierContext &context);
  void verify();
  void verifyType(HIRTypeDecl *type);
  void verifyMethod(HIRMethodDecl *method, HIRTypeDecl *type);
  void verifyParam(HIRParam *param);
  void verifyLocal(HIRLocal *local);
  void verifyBlock(HIRBlockStmt *stmt);
  void verifyStmt(HIRStmt *stmt);
  void verifyExpr(HIRExpr *expr, bool isRead = true);
  void verifyIf(HIRIfStmt *stmt);
  void verifyCasePattern(HIRCasePattern *pattern);

  bool definitelyReturns(HIRStmt *stmt);
};