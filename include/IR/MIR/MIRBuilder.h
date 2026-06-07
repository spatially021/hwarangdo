#pragma once

#include "IR/HIR/HIRDecl.h"
#include "IR/HIR/HIRExpr.h"
#include "IR/HIR/HIRProgram.h"
#include "IR/HIR/HIRStmt.h"
#include "IR/HIR/HIRType.h"
#include "IR/MIR/MIRExpr.h"
#include "IR/MIR/MIRInst.h"
#include "IR/MIR/MIRNode.h"
#include "IR/MIR/MIRProgram.h"
#include "IR/MIR/MIRType.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "magic_enum/magic_enum.hpp"
#include <memory>

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

  MIRBuilder(HIRProgram *hp, MIRProgram *mp) : HirProgram(hp), program(mp) {}

  void build();

private:
  MIRFunction *currentFunc = nullptr;
  BlockID currentBlock = 0;

  void emit(unique_ptr<MIRInst> inst);

private:
  BlockID makeBlock();
  BasicBlock *getBlock(BlockID id);
  bool hasTerminator(BlockID id);

private:
  MIRType *getType(TypeSymbol *type);
  MIRType *getType(HIRType *type);

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

  unique_ptr<MIRExpr> lowerExpr(HIRExpr *expr);
};