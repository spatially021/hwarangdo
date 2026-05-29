
#include "IR/HIR/HIRDecl.h"
#include "IR/HIR/HIRExpr.h"
#include "IR/HIR/HIRPattern.h"
#include "IR/HIR/HIRProgram.h"
#include "IR/HIR/HIRStmt.h"
#include "IR/HIR/HIRSymbol.h"
#include "enums/InheritState.h"
#include <magic_enum/magic_enum.hpp>
#include <string>

struct InitMap {
  std::unordered_map<HIRLocal *, InitState> localStates;
  std::unordered_map<HIRField *, InitState> fieldStates;
  std::unordered_map<HIRField *, InitState> rootStates;
  std::unordered_map<HIRParam *, InitState> paramStates;
};

template <typename T> T *expect(HIRNode *node, HIRNodeKind expected) {
  if (node == nullptr) {
    Error::internal("expected " + std::string(magic_enum::enum_name(expected)) +
                    ", but got nullptr");
  }

  if (node->kind != expected) {
    Error::internal("expected " + std::string(magic_enum::enum_name(expected)) +
                    ", but got " +
                    std::string(magic_enum::enum_name(node->kind)));
  }

  auto *casted = dynamic_cast<T *>(node);
  if (casted == nullptr) {
    Error::internal("failed to cast node. expected kind " +
                    std::string(magic_enum::enum_name(expected)));
  }

  return casted;
}

class HIRVerifier {

private:
  InitMap initmap;
  unordered_map<HIRTypeDecl *, InheritState> inheritStates;

public:
  HIRProgram *program = nullptr;
  HIRVerifier(HIRProgram *p);
  void verify();
  void verifyType(HIRTypeDecl *type);
  void verifyMethod(HIRMethodDecl *method);
  void verifyParam(HIRParam *param);
  void verifyLocal(HIRLocal *local);
  void verifyBlock(HIRBlockStmt *stmt);
  void verifyStmt(HIRStmt *stmt);
  void verifyExpr(HIRExpr *expr, bool isRead = true);
  void verifyIf(HIRIfStmt *stmt);
  void verifyField(HIRField *field);
  void verifyRoot(HIRField *root);
  void verifyVariant(HIREnumVariant *variant);
  void verifyCasePattern(HIRCasePattern *pattern);

  void checkInitialize(HIRPlaceExpr *place);
  void initialize(HIRPlaceExpr *place);
  InitState &getInitState(HIRPlaceExpr *place);
};