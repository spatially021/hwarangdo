
#include "hrd/AST/Decl.h"
#include "hrd/AST/Expr.h"
#include "hrd/IR/HIR/HIRBuilder.h"
#include "hrd/IR/HIR/HIRDecl.h"
#include "hrd/IR/HIR/HIRExpr.h"
#include "hrd/IR/HIR/HIRProgram.h"
#include "hrd/IR/HIR/HIRStmt.h"
#include "hrd/IR/HIR/HIRSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/SourceSpan.h"
#include "hrd/util/Error.h"
#include "hrd/util/Guard.h"
#include <cassert>
#include <memory>
#include <utility>
using std::unique_ptr;

unique_ptr<HIRSelfExpr> HIRBuilder::lowerImplictSelf() {
  assert(currentType);

  auto type = currentType->type;
  SourceSpan span;
  return make_unique<HIRSelfExpr>(
      span,
      type->kind == TypeKind::STRUCT ? HIRSelfKind::Self : HIRSelfKind::This,
      type, type, type);
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
  if (currentMethod == nullptr) {
    Error::internal("currentMethod is nullptr");
  }
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

void HIRBuilder::setDefaultInit(HIRTypeDecl *type) {
  type->defaultInitBlock = make_unique<HIRBlockStmt>(type->span);
  auto block = type->defaultInitBlock.get();

  for (auto &f : type->defaultInit) {
    auto ty = type->type;
    auto span = f.second->span;
    auto place = make_unique<HIRFieldPlaceExpr>(
        span,
        make_unique<HIRSelfExpr>(span,
                                 ty->kind == TypeKind::STRUCT
                                     ? HIRSelfKind::Self
                                     : HIRSelfKind::This,
                                 ty, ty, ty),
        f.first);
    auto rhs = lowerValue(f.second);
    auto assign =
        make_unique<HIRAssignStmt>(span, std::move(place), std::move(rhs));
    block->statements.push_back(std::move(assign));
  }
}

void HIRBuilder::bindMethod(FuncDecl *decl) {
  auto it = program->typeDeclMap.find(decl->methodSymbol->owner);
  if (it == program->typeDeclMap.end()) {
    Error::internal(decl->span, "fail to find method's owner type");
  }
  auto type = it->second;

  HIRMethodDecl *method = nullptr;

  switch (decl->methodSymbol->methodKind) {

  case MethodKind::Normal: {
    auto mIT = type->methodMap.find(decl->methodSymbol);
    if (mIT == type->methodMap.end()) {
      Error::internal(decl->span, "fail to find method");
    }
    method = mIT->second;
    break;
  }
  case MethodKind::Init: {
    auto iIt = type->initMap.find(decl->methodSymbol);
    if (iIt == type->initMap.end()) {
      Error::internal(decl->span, "fail to find init method");
    }
    method = iIt->second;
    break;
  }

  case MethodKind::OnDestroy: {
    method = type->onDestroy;
    break;
  }
  }
  if (method == nullptr) {
    Error::internal(decl->span, "fail to get method");
  }

  MethodGuard _(currentMethod, method);
  if (decl->body != nullptr) {
    method->body = lowerStmtAsBlock(decl->body.get());
  }
}

pair<bool, HIRLocal *> HIRBuilder::lookupLocal(ValueSymbol *symbol) {

  for (auto cb = currentBlock; cb != nullptr; cb = cb->parent) {
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

bool HIRBuilder::isTypeReceiver(Expr *expr) {
  if (auto name = dynamic_cast<NameExpr *>(expr)) {
    return name->resolved->type == Symbol::SymbolType::TYPE;
  }
  return false;
}

pair<bool, HIRMethodDecl *> HIRBuilder::lookupMethod(HIRTypeDecl *type,
                                                     MethodSymbol *symbol) {
  auto it = type->methodMap.find(symbol);
  bool b = it != type->methodMap.end();
  return {b, b ? it->second : nullptr};
}

pair<bool, HIRMethodDecl *> HIRBuilder::lookupInit(HIRTypeDecl *type,
                                                   MethodSymbol *symbol) {
  auto it = type->initMap.find(symbol);
  bool b = it != type->initMap.end();
  return {b, b ? it->second : nullptr};
}