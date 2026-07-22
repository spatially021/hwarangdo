#pragma once

#include "hrd/IR/HIR/HIRDecl.h"
#include "hrd/IR/HIR/HIRExpr.h"
#include "hrd/IR/HIR/HIRProgram.h"
#include "hrd/IR/HIR/HIRStmt.h"
#include "hrd/IR/HIR/HIRType.h"
#include "hrd/IR/IRScope.h"
#include "hrd/IR/MIR/MIRExpr.h"
#include "hrd/IR/MIR/MIRNode.h"
#include "hrd/IR/MIR/MIRProgram.h"
#include "hrd/SemanticAnalyzer/SymbolTable.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/compiler/CompilerContexts.h"
#include "magic_enum/magic_enum.hpp"
#include <memory>

struct LoopContext {
  BlockID continueTarget = 0;
  BlockID breakTarget = 0;
  IRScope *breakScope;
  IRScope *continueScope;
};

struct MatchContext {
  ValueSymbol *result = nullptr;
  BlockID join;
};

struct SwitchData {
  BlockID cond;
  BlockID defaultTarget;
  BlockID cleanup;
  BlockID join;
  IRScope &scope;
  HIRValueExpr *condExpr = nullptr;
  vector<unique_ptr<HIRCase>> &cases;
  TypeSymbol *type = nullptr;
};

class MIRBuilder {

  template <typename T> T *expect(HIRNode *node, HIRNodeKind expected) {
    if (node == nullptr) {
      Error::internal("expected " +
                      std::string(magic_enum::enum_name(expected)) +
                      ", but got nullptr");
    }

    if (node->kind != expected) {
      Error::internal(
          "expected " + std::string(magic_enum::enum_name(expected)) +
          ", but got " + std::string(magic_enum::enum_name(node->kind)));
    }

    auto *casted = dynamic_cast<T *>(node);
    if (casted == nullptr) {
      Error::internal("failed to cast node. expected kind " +
                      std::string(magic_enum::enum_name(expected)));
    }

    return casted;
  }

public:
  HIRProgram *HirProgram = nullptr;
  MIRProgram *program = nullptr;
  SymbolTable &table;

  MIRBuilder(MIRContext &ctx)
      : HirProgram(ctx.hirProgram), program(ctx.mirProgram), table(ctx.table) {}

  void build();

private:
  MIRFunction *currentFunc = nullptr;
  IRScope *currentScope = nullptr;
  BlockID currentBlock = 0;
  vector<LoopContext> loops;
  vector<MatchContext> matches;
  vector<unique_ptr<IRScope>> scopes;
  void emit(unique_ptr<MIRStmt> inst);

private:
  BlockID makeBlock();
  BasicBlock *getBlock(BlockID id);
  bool hasTerminator(BlockID id);
  ValueSymbol *makeTemp(TypeSymbol *type);

private:
  void lowerType(HIRTypeDecl *type);
  void lowerMethod(TypeSymbol *owner, HIRMethodDecl *method);
  void lowerBlock(HIRBlockStmt *block);

  void lowerStmt(HIRStmt *stmt);
  void lowerExprStmt(HIRExprStmt *stmt);
  void lowerIf(HIRIfStmt *stmt);
  void lowerWhile(HIRWhileStmt *stmt);
  void lowerForRange(HIRForRangeStmt *stmt);
  void lowerAssign(HIRAssignStmt *stmt);
  void lowerCompoundAssign(HIRCompoundAssignStmt *stmt);
  void lowerBreak(HIRBreakStmt *stmt);
  void lowerContinue(HIRContinueStmt *stmt);
  void loewrLocalDecl(HIRLocalDeclStmt *stmt);
  void lowerQuit(HIRQuitStmt *stmt);
  void lowerReturn(HIRReturnStmt *stmt);
  void lowerSwitch(HIRSwitchStmt *stmt);
  void lowerValueTransfer(HIRValueTransferStmt *stmt);
  void lowerDestroy(HIRDestroyStmt *stmt);

  unique_ptr<MIRValue> lowerExpr(HIRExpr *expr);
  unique_ptr<MIRValue> lowerTernary(HIRTernaryExpr *expr);
  unique_ptr<MIRValue> lowerMatch(HIRMatchExpr *expr);
  unique_ptr<MIRValue> lowerLiteral(HIRLiteralExpr *expr);
  unique_ptr<MIRValue> lowerLoad(HIRLoadExpr *expr);
  std::unique_ptr<MIRValue> lowerUnary(HIRUnaryExpr *expr);
  unique_ptr<MIRValue> lowerBinary(HIRBinaryExpr *expr);
  unique_ptr<MIRValue> lowerCast(HIRCastExpr *expr);
  unique_ptr<MIRValue> lowerCall(HIRMethodCallExpr *expr);
  unique_ptr<MIRValue> lowerSpawn(HIRSpawnExpr *expr);
  unique_ptr<MIRValue> lowerView(HIRViewExpr *expr);
  unique_ptr<MIRValue> lowerStructInit(HIRStructInitExpr *expr);
  unique_ptr<MIRValue> lowerVariantValue(HIRVariantValueExpr *expr);
  unique_ptr<MIRValue> lowerSelf(HIRSelfExpr *expr);
  unique_ptr<MIRValue> lowerRuntime(HIRRuntimeCall *expr);

  unique_ptr<MIRPlace> lowerPlace(HIRPlaceExpr *expr);
  unique_ptr<MIRLocalPlace> lowerLocal(HIRLocalPlaceExpr *expr);
  unique_ptr<MIRParamPlace> lowerParam(HIRParamPlaceExpr *expr);
  unique_ptr<MIRArrayAccessPlace> lowerArray(HIRArrayAccessPlaceExpr *expr);
  unique_ptr<MIRFieldPlace> lowerField(HIRFieldPlaceExpr *expr);
  unique_ptr<MIRPlace> lowerReceiverToPlace(HIRExpr *receiver);

  IRScope *enterScope();
  void exitScope();
  void emitCleanup(IRScope *scope);
  void emitCleanupUntil(IRScope *toExclusive);

  void makeSwitch(SwitchData &data);
};