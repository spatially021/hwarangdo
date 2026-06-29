#pragma once

#include "hrd/IR/MIR/MIRExpr.h"
#include "hrd/IR/MIR/MIRNode.h"
#include "hrd/IR/MIR/MIRProgram.h"
#include "hrd/IR/MIR/MIRStmt.h"
#include "hrd/SemanticAnalyzer/SymbolTable.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/RuntimeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Metadata.h>

#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <memory>
#include <unordered_map>

using BlockMap = unordered_map<BlockID, llvm::BasicBlock *>;
using localMap = unordered_map<ValueSymbol *, llvm::AllocaInst *>;
using ParamMap = unordered_map<ValueSymbol *, llvm::Value *>;

struct FuncContext {
  llvm::Function *func;
  BlockMap blocks;
  localMap locals;
  ParamMap params;
};

class llvmCodegen {

public:
  llvm::LLVMContext context;
  MIRProgram *program = nullptr;
  SymbolTable *table = nullptr;
  std::unique_ptr<llvm::Module> llvmModule;
  llvm::IRBuilder<> builder;

  unordered_map<TypeSymbol *, llvm::Type *> types;
  unordered_map<ValueSymbol *, llvm::GlobalVariable *> roots;
  std::unordered_map<MethodSymbol *, llvm::Function *> funcs;
  unordered_map<RuntimeSymbol *, llvm::Function *> runtimes;

  llvmCodegen(MIRProgram *program, SymbolTable *table);
  void generate();

private:
  llvm::GlobalVariable *rootGlobal = nullptr;

private:
  void buildTypes();
  void buildMethods();

  void declareRoots(Scope *rootScope);

  void lowerBlock(BasicBlock *block, FuncContext &ctx);
  void lowerStmt(MIRStmt *stmt, FuncContext &ctx);
  void lowerExprStmt(MIRExprStmt *stmt, FuncContext &ctx);
  void lowerAssign(MIRAssignStmt *stmt, FuncContext &ctx);
  void lowerLocalDecl(MIRLocalDeclStmt *stmt, FuncContext &ctx);

  void lowerTerminator(MIRTerminator &terminator, FuncContext &ctx);

  llvm::Value *lowerValue(MIRValue *value, FuncContext &ctx);

  llvm::Value *lowerPlace(MIRPlace *place, FuncContext &ctx);

  void emitFuncBody(MIRFunction *func);
  string mangle(MethodSymbol *symbol);

  llvm::Value *lowerBinaryExpr(MIRBinaryExpr *expr, FuncContext &ctx);
  llvm::Value *lowerLoad(MIRLoad *expr, FuncContext &ctx);
  llvm::Value *lowerPayloadExtractExpr(MIRPayloadExtractExpr *expr,
                                       FuncContext &ctx);
  llvm::Value *lowerLiteralExpr(MIRLiteralExpr *expr, FuncContext &ctx);
  llvm::Value *lowerStringLiteral(const StringPayload &payload,
                                  TypeSymbol *type);
  llvm::Value *lowerUnaryExpr(MIRUnaryExpr *expr, FuncContext &ctx);
  llvm::Value *lowerCastExpr(MIRCastExpr *expr, FuncContext &ctx);
  llvm::Value *lowerCallExpr(MIRCallExpr *expr, FuncContext &ctx);
  llvm::Value *lowerSpawnExpr(MIRSpawnExpr *expr, FuncContext &ctx);
  llvm::Value *lowerViewExpr(MIRViewExpr *expr, FuncContext &ctx);
  llvm::Value *lowerStructInitExpr(MIRStructInitExpr *expr, FuncContext &ctx);
  llvm::Value *lowerVariantExpr(MIRVariantExpr *expr, FuncContext &ctx);

  llvm::Value *lowerLogicalAnd(MIRBinaryExpr *expr, FuncContext &ctx);
  llvm::Value *lowerLogicalOr(MIRBinaryExpr *expr, FuncContext &ctx);

  llvm::Value *lowerLocalPlace(MIRLocalPlace *place, FuncContext &ctx);
  llvm::Value *lowerParamPlace(MIRParamPlace *place, FuncContext &ctx);
  llvm::Value *lowerArrayAccessPlace(MIRArrayAccessPlace *place,
                                     FuncContext &ctx);
  llvm::Value *lowerFieldPlace(MIRFieldPlace *place, FuncContext &ctx);
  llvm::Value *lowerRootPlace(MIRRootPlace *place, FuncContext &ctx);
  llvm::Value *lowerRuntime(MIRRuntimeCallExpr *expr, FuncContext &ctx);
  llvm::Function *getOrDeclareRuntimeFunction(RuntimeSymbol *rt);

  llvm::AllocaInst *createEntryAlloca(llvm::Function *fn, llvm::Type *ty,
                                      llvm::StringRef name);

  llvm::Type *getType(TypeSymbol *type);
  llvm::Type *buildPrimitiveType(PrimtiveType *type);
  llvm::Value *castTo(llvm::Value *v, TypeSymbol *from, TypeSymbol *to);

  void lowerStructInitTo(MIRStructInitExpr *expr, llvm::Value *dst,
                         FuncContext &ctx);

  bool isUnsigned(TypeSymbol *type);
  bool isFloat(TypeSymbol *type);
  bool isInt(TypeSymbol *type);

  uint64_t arrayLengthToU64(const llvm::APInt &v);

  void generateEntryMain(MainSymbol *mainType);
  llvm::Function *getRuntimeFunc(const std::string &name,
                                 llvm::FunctionType *type);
  MethodSymbol *getMainMethod(const std::string &name);
};