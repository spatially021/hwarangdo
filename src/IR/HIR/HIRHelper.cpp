
#include "AST/Decl.h"
#include "IR/HIR/HIRBuilder.h"
#include "IR/HIR/HIRDecl.h"
#include "IR/HIR/HIRExpr.h"
#include "IR/HIR/HIRSymbol.h"
#include "IR/HIR/HIRType.h"
#include "SemanticAnalyzer/symbol/MethodSymbol.h"
#include "SemanticAnalyzer/symbol/ValueSymbol.h"
#include "util/Error.h"
#include "util/Guard.h"
#include <cassert>
#include <memory>
#include <utility>
using std::unique_ptr;

unique_ptr<HIRSelfExpr> HIRBuilder::lowerImplictSelf() {
  assert(currentType);

  auto type = currentType->type;

  return make_unique<HIRSelfExpr>(
      type->kind == HIRTypeKind::Struct ? HIRSelfKind::Self : HIRSelfKind::This,
      type, type, type);
}

HIRLocal *HIRBuilder::makeTemp(HIRType *type) {
  auto local = make_unique<HIRLocal>();
  local->id = allocLocalID();
  local->type = type;
  local->symbol = nullptr;
  local->kind = HIRLocalKind::Temp;
  local->isMutable = false;
  local->isInitialized = false;
  local->name = "";
  auto raw = local.get();

  currentMethod->locals.push_back(std::move(local));

  return raw;
}

HIRLocal *HIRBuilder::lookUpLocal(ValueSymbol *symbol) {
  assert(currentBlock);
  auto it = currentBlock->localMap.find(symbol);
  if (it == currentBlock->localMap.end()) {
    return nullptr;
  }
  return it->second;
}

unique_ptr<HIRValueExpr> HIRBuilder::lowerValue(Expr *expr) {
  exprResult = lowerExpr(expr);
  unique_ptr<HIRValueExpr> rt = nullptr;
  if (dynamic_cast<HIRValueExpr *>(exprResult.get())) {
    rt = unique_ptr<HIRValueExpr>(
        static_cast<HIRValueExpr *>(exprResult.release()));
  } else if (dynamic_cast<HIRPlaceExpr *>(exprResult.get())) {
    rt = load(unique_ptr<HIRPlaceExpr>(
        static_cast<HIRPlaceExpr *>(exprResult.release())));
  } else {
    Error::internal("expect value or place type");
  }
  if (rt == nullptr) {
    Error::internal(expr->span, "fail to get value");
  }
  return rt;
}

int HIRBuilder::allocLocalID() {
  assert(currentMethod);
  return currentMethod->nextLocalId++;
}

int HIRBuilder::allocMethodID() {
  assert(currentType);
  return currentType->nextMethodID++;
}

int HIRBuilder::allocParamID() {
  if (currentMethod == nullptr) {
    Error::internal("currentMethod is nullptr");
  }
  return currentMethod->nextParamID++;
}

int HIRBuilder::allocFieldID() {
  assert(currentType);
  return currentType->nextFieldId++;
}

int HIRBuilder::allocRootID() {
  assert(program);
  return program->nextRootId++;
}

void HIRBuilder::bindLocal(ValueSymbol *symbol, unique_ptr<HIRLocal> local) {
  assert(currentBlock);
  assert(currentMethod);
  currentBlock->localMap.emplace(symbol, local.get());
  currentMethod->locals.push_back(std::move(local));
}

void HIRBuilder::bindMethod(FuncDecl *decl) {
  auto it = program->typeDeclMap.find(decl->methodSymbol->onwer);
  if (it == program->typeDeclMap.end()) {
    Error::internal(decl->span, "fail to find method's owner type");
  }
  auto type = it->second;
  auto mIT = type->methodMap.find(decl->methodSymbol);
  if (mIT == type->methodMap.end()) {
    Error::internal(decl->span, "fail to find method");
  }
  auto method = mIT->second;
  MethodGuard _(currentMethod, method);
  method->body = lowerStmtAsBlock(decl->body.get());
}

void HIRBuilder::bindField(ValueSymbol *symbol, unique_ptr<HIRField> field) {

  assert(currentType);
  currentType->fieldMap.emplace(symbol, field.get());
  currentType->fields.push_back(std::move(field));
}

pair<bool, HIRLocal *> HIRBuilder::lookupLocal(ValueSymbol *symbol) {

  for (auto cb = currentBlock; currentBlock != nullptr;
       cb = currentBlock->parent) {
    auto it = cb->localMap.find(symbol);
    bool b = it != cb->localMap.end();
    if (b) {
      return {b, it->second};
    }
  }
  return {false, nullptr};
}

pair<bool, HIRParam *> HIRBuilder::lookupParam(ValueSymbol *symbol) {
  auto it = currentMethod->paramMap.find(symbol);
  bool b = it != currentMethod->paramMap.end();
  return {b, b ? it->second : nullptr};
}

pair<bool, HIRField *> HIRBuilder::lookupField(ValueSymbol *symbol) {
  auto it = currentType->fieldMap.find(symbol);
  bool b = it != currentType->fieldMap.end();
  return {b, b ? it->second : nullptr};
}

pair<bool, HIRField *> HIRBuilder::lookupField(HIRTypeDecl *type,
                                               ValueSymbol *symbol) {
  auto it = type->fieldMap.find(symbol);
  bool b = it != type->fieldMap.end();
  return {b, b ? it->second : nullptr};
}

bool HIRBuilder::isTypeReceiver(Expr *expr) {
  if (auto name = dynamic_cast<NameExpr *>(expr)) {
    return name->resolved->type == Symbol::SymbolType::TYPE;
  }
  return false;
}

pair<bool, HIREnumVariant *>
HIRBuilder::lookupVariant(EnumVariantSymbol *symbol) {
  auto it = program->variantMap.find(symbol);
  return {it != program->variantMap.end(), it->second};
}

pair<bool, HIRMethodDecl *> HIRBuilder::lookupMethod(HIRTypeDecl *type,
                                                     MethodSymbol *symbol) {
  auto it = type->methodMap.find(symbol);
  bool b = it != type->methodMap.end();
  return {b, b ? it->second : nullptr};
}
