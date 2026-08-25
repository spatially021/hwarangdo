#pragma once

#include "hrd/IR/MIR/MIRExpr.h"
#include "hrd/IR/MIR/MIRNode.h"
#include "hrd/IR/MIR/MIRProgram.h"
#include "hrd/IR/MIR/MIRStmt.h"
#include "hrd/SemanticAnalyzer/ResolvedLit.h"
#include "hrd/SemanticAnalyzer/SymbolTable/SymbolTable.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/RuntimeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/compiler/CompilerContexts.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Metadata.h>

#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <memory>
#include <unordered_map>
#include <vector>

using BlockMap = unordered_map<BlockID, llvm::BasicBlock *>;
using localMap = unordered_map<ValueSymbol *, llvm::AllocaInst *>;
using ParamMap = unordered_map<ValueSymbol *, llvm::Value *>;

struct Cleanup {
  llvm::Value *addr = nullptr;
  TypeSymbol *type = nullptr;
};
struct FuncContext {
  llvm::Function *func;
  BlockMap blocks;
  localMap locals;
  ParamMap params;
  llvm::Value *self = nullptr;
  std::vector<Cleanup> cleanupStack;
  std::unordered_set<llvm::Value *> canceledCleanups;
};

struct LoweredValue {
  llvm::Value *value = nullptr; // 실제 SSA value
  llvm::Value *addr = nullptr;  // cleanup/release 가능한 주소
  MIRValueCategory category = MIRValueCategory::Plain;
};

class llvmCodegen {
  using Str = const string &;

public:
  llvm::LLVMContext context;
  MIRProgram *program = nullptr;
  SymbolTable &table;
  std::unique_ptr<llvm::Module> llvmModule;
  llvm::IRBuilder<> builder;

  unordered_map<TypeSymbol *, llvm::Type *> types;
  unordered_map<TypeSymbol *, llvm::Type *> enums;
  unordered_map<ValueSymbol *, llvm::GlobalVariable *> roots;
  std::unordered_map<MethodSymbol *, llvm::Function *> funcs;
  unordered_map<RuntimeSymbol *, llvm::Function *> runtimes;
  unordered_map<TypeSymbol *, llvm::Function *> defaultInits;
  unordered_map<TypeSymbol *, llvm::Function *> defaultDestroys;

  llvmCodegen(CodegenContext &context);
  void generate();
  bool emitObject(const std::filesystem::path &outputPath);

private:
  bool isCompile;

private:
  llvm::GlobalVariable *rootGlobal = nullptr;
  llvm::StructType *hrdHandleTy = nullptr;

private:
  void buildTypes();
  void buildMethods();

  void declareRoots(Scope *rootScope);

  void lowerBlock(BasicBlock *block, FuncContext &ctx);
  void lowerStmt(MIRStmt *stmt, FuncContext &ctx);
  void lowerExprStmt(MIRExprStmt *stmt, FuncContext &ctx);
  void lowerCleanup(MIRCleanupStmt *stmt, FuncContext &ctx);

  void lowerAssign(MIRAssignStmt *stmt, FuncContext &ctx);
  void lowerStringAssign(Str type, llvm::Value *dstPtr, llvm::Value *srcValue,
                         MIRValueCategory category);
  void lowerStringCopyAssign(Str type, llvm::Value *dst, llvm::Value *srcPtr);
  void lowerStringMoveAssign(Str type, llvm::Value *dst, llvm::Value *srcPtr);
  void lowerStructAssign(TypeSymbol *ty, llvm::Value *dst, LoweredValue rhs,
                         MIRValueCategory category, FuncContext &ctx);
  void lowerEnumMoveAssign(TypeSymbol *type, llvm::Value *dst,
                           llvm::Value *src);

  void lowerEnumCopyAssign(TypeSymbol *type, llvm::Value *dst, llvm::Value *src,
                           FuncContext &ctx);

  void lowerLocalDecl(MIRLocalDeclStmt *stmt, FuncContext &ctx);
  void lowerQuit(MIRQuitStmt *stmt, FuncContext &ctx);

  void assign(llvm::Value *lhs, LoweredValue rhs, TypeSymbol *type,
              FuncContext &ctx);

  void lowerStringSwitch(const SwitchTerminator &t, FuncContext &ctx);
  void lowerNativeSwitch(const SwitchTerminator &t, FuncContext &ctx);
  void lowerEnumSwitch(const SwitchTerminator &t, FuncContext &ctx);

  void lowerDestroy(MIRDestroyStmt *stmt, FuncContext &ctx);
  llvm::FunctionCallee getOrDeclareWorldDestroyRaw();

  void lowerTerminator(MIRTerminator &terminator, FuncContext &ctx);

  LoweredValue lowerValue(MIRValue *value, FuncContext &ctx);
  llvm::Value *lowerPlace(MIRPlace *place, FuncContext &ctx);

  vector<Cleanup> lowerArgs(vector<llvm::Value *> &args,
                            vector<MIRValue *> values, FuncContext &ctx);
  llvm::Value *lowerReceiverPtr(MIRPlace *place, FuncContext &ctx);

  void emitFuncBody(MIRFunction *func);
  string mangle(MethodSymbol *symbol);
  string mangleType(TypeSymbol *type);

  LoweredValue lowerBinaryExpr(MIRBinaryExpr *expr, FuncContext &ctx);
  LoweredValue lowerLoad(MIRLoad *expr, FuncContext &ctx);
  LoweredValue lowerPayloadExtractExpr(MIRPayloadExtractExpr *expr,
                                       FuncContext &ctx);
  LoweredValue lowerLiteralExpr(MIRLiteralExpr *expr, FuncContext &ctx);
  LoweredValue lowerStringLiteral(const StringPayload &payload,
                                  TypeSymbol *type);
  LoweredValue lowerS8(const StringPayload &payload, StringType *type);
  LoweredValue lowerS16(const StringPayload &payload, StringType *type);
  LoweredValue lowerS32(const StringPayload &payload, StringType *type);

  LoweredValue lowerUnaryExpr(MIRUnaryExpr *expr, FuncContext &ctx);
  LoweredValue lowerCastExpr(MIRCastExpr *expr, FuncContext &ctx);
  LoweredValue lowerCallExpr(MIRCallExpr *expr, FuncContext &ctx);

  LoweredValue lowerSpawnExpr(MIRSpawnExpr *expr, FuncContext &ctx);
  llvm::FunctionCallee getOrDeclareWorldSpawnRaw();

  LoweredValue lowerViewExpr(MIRViewExpr *expr, FuncContext &ctx);
  llvm::FunctionCallee getOrDeclareWorldViewRaw();

  LoweredValue lowerStructInitExpr(MIRStructInitExpr *expr, FuncContext &ctx);
  LoweredValue lowerVariantExpr(MIRVariantExpr *expr, FuncContext &ctx);

  LoweredValue lowerLogicalAnd(MIRBinaryExpr *expr, FuncContext &ctx);
  LoweredValue lowerLogicalOr(MIRBinaryExpr *expr, FuncContext &ctx);

  llvm::Value *lowerLocalPlace(MIRLocalPlace *place, FuncContext &ctx);
  llvm::Value *lowerParamPlace(MIRParamPlace *place, FuncContext &ctx);
  llvm::Value *lowerArrayAccessPlace(MIRArrayAccessPlace *place,
                                     FuncContext &ctx);
  llvm::Value *lowerFieldPlace(MIRFieldPlace *place, FuncContext &ctx);
  llvm::Value *lowerRootPlace(MIRRootPlace *place, FuncContext &ctx);
  LoweredValue lowerRuntime(MIRRuntimeCallExpr *expr, FuncContext &ctx);
  llvm::Function *getOrDeclareRuntimeFunction(RuntimeSymbol *rt);
  llvm::Function *getOrgetOrDeclareFunction(MethodSymbol *method);
  llvm::AllocaInst *createEntryAlloca(llvm::Function *fn, llvm::Type *ty,
                                      llvm::StringRef name);

  llvm::Type *getType(TypeSymbol *type);
  llvm::Type *getLayoutType(TypeSymbol *type);
  llvm::Type *getFieldType(TypeSymbol *type);

  llvm::Type *buildArrayType(ArrayTypeSymbol *arr);
  void declareArrayDestroy(ArrayTypeSymbol *arr);
  void emitArrayDestroy(ArrayTypeSymbol *arr);
  std::string getArrayDestroyName(ArrayTypeSymbol *arr);

  llvm::Type *buildPrimitiveType(PrimtiveType *type);
  LoweredValue castTo(LoweredValue value, TypeSymbol *from, TypeSymbol *to);

  void lowerStructInitTo(MIRStructInitExpr *expr, llvm::Value *dst,
                         FuncContext &ctx);

  bool isUnsigned(TypeSymbol *type);
  bool isFloat(TypeSymbol *type);
  bool isInt(TypeSymbol *type);
  bool isString(TypeSymbol *type);

  llvm::Value *getSizeOf(llvm::Type *type);
  llvm::FunctionCallee getOrDeclareMalloc();

  llvm::Type *getHrdHandleType();

  uint64_t arrayLengthToU64(const llvm::APInt &v);

  void generateEntryMain(MainSymbol *mainType);
  llvm::Function *getRuntimeFunc(const std::string &name,
                                 llvm::FunctionType *type);
  MethodSymbol *getMainMethod(const std::string &name);
  void emitFieldZeroInit(TypeSymbol *owner, llvm::Value *self);
  void addClean(llvm::Value *addr, TypeSymbol *type, FuncContext &ctx);
  // void emitCleanups(FuncContext &ctx);
  llvm::Function *emitDefaultDestroy(TypeSymbol *ty);
  bool needsDestroy(TypeSymbol *type);

  llvm::Value *materializeAddress(const LoweredValue &value,
                                  llvm::Type *layoutType, llvm::StringRef name);

  llvm::Value *extractEnumTag(const LoweredValue &value, TypeSymbol *enumType);
  std::string getStringSuffix(TypeSymbol *type);
};
