#pragma once

#include "hrd/IR/HIR/HIRDecl.h"
#include "hrd/IR/HIR/HIRExpr.h"
#include "hrd/IR/HIR/HIRPattern.h"
#include "hrd/IR/HIR/HIRProgram.h"
#include "hrd/IR/HIR/HIRStmt.h"
#include "hrd/InitChecker/InitSummary.h"
#include "hrd/Recover/InitCheckerRecover.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/compiler/CompilerContexts.h"
#include "hrd/diagnostic/DiagnosticEngine.h"

#include <unordered_map>
#include <unordered_set>

using InitSet = std::unordered_set<ValueSymbol *>;
using FieldSet = std::unordered_set<ValueSymbol *>;

struct InitState {
  InitSet values;

  // struct local/param -> 현재 보장되는 field
  std::unordered_map<ValueSymbol *, FieldSet> fields;
};

class InitChecker {
public:
  InitChecker(InitChecerContext &ctx);

  void check();
  [[nodiscard]]
  InitSummary getSummary() const;

private:
  void checkType(HIRTypeDecl *type);
  void checkInit(HIRMethodDecl *method);
  void checkMethod(HIRMethodDecl *method);

  void checkBlock(HIRBlockStmt *block, InitState &state);
  void checkStmt(HIRStmt *stmt, InitState &state);
  void checkExpr(HIRExpr *expr, InitState &state);

  void checkRead(HIRExpr *expr, InitState &state);
  void checkWrite(HIRExpr *expr, InitState &state);

  void checkCase(HIRCase *caseStmt, InitState &state);
  void checkCasePattern(HIRCasePattern *pattern, InitState &state);

private:
  InitState mergeInit(const InitState &lhs, const InitState &rhs);
  FieldSet mergeField(const FieldSet &lhs, const FieldSet &rhs);

  FieldSet getExprFields(HIRExpr *expr, const InitState &state);

  ValueSymbol *getValueSymbol(HIRExpr *expr);

  bool contain(const InitSet &set, ValueSymbol *symbol);

  void requireInitialized(ValueSymbol *symbol, HIRExpr *expr,
                          const InitState &state);

  void requireField(ValueSymbol *owner, ValueSymbol *field, HIRExpr *expr,
                    const InitState &state);

  void requireSelfField(ValueSymbol *field, HIRExpr *expr);

  void addMethodEntryState(HIRMethodDecl *method, InitState &state);

  void prepareImportedSummary();

private:
  HIRProgram *program = nullptr;
  HIRTypeDecl *currentType = nullptr;

  bool checkingInit = false;

  // 현재 검사 중인 init에서 self에 지금까지 초기화된 field
  FieldSet currentSelfFields;

  // init -> 해당 init이 보장하는 field
  std::unordered_map<MethodSymbol *, FieldSet> initFields;

  // type -> 모든 init이 공통으로 보장하는 field
  std::unordered_map<TypeSymbol *, FieldSet> commonFields;

  DiagnosticEngine &engine;
  InitCheckerRecover recover = InitCheckerRecover(*this);
};