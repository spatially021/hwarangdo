#include "hrd/AST/ASTNode.h"
#include "hrd/AST/Decl.h"
#include "hrd/AST/DeclContext.h"
#include "hrd/AST/Expr.h"
#include "hrd/AST/Stmt.h"
#include "hrd/SemanticAnalyzer/ResolvedLit.h"
#include "hrd/SemanticAnalyzer/Resolver.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/Symbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/diagnostic/Diagnostic.h"
#include "hrd/util/Error.h"
#include "hrd/util/Guard.h"
#include "hrd/util/Helper.h"
#include "hrd/util/TypeResolver.h"
#include <llvm/ADT/APInt.h>
#include <string>

void Resolver::visit(ClassDecl *decl) {
  if (!decl->symbol) {
    Error::internal(decl->span, "ClassDecl symbol not initialized");
  }
  ScopeGuard _(table, decl->symbol->memberScope);
  TypeContextGuard __(currentType, decl->symbol);

  for (auto a : decl->fields)
    a->accept(this);
  for (auto a : decl->methods) {
    a->accept(this);
  }

  if (auto [result, type] = Helper::checkImplementTraitSig(decl->symbol);
      !result) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S034);
    dia.labels = {{decl->span,
                   "type '" + decl->name + "' does not implement trait '" +
                       type->name + "'",
                   true}};
    engine.emit(dia);
    recover.recover();
  }
}

void Resolver::visit(StructDecl *decl) {
  if (!decl->symbol) {
    Error::internal(decl->span, "StructDecl symbol not initialized");
  }
  ScopeGuard _(table, decl->symbol->memberScope);
  TypeContextGuard __(currentType, decl->symbol);
  for (auto &a : decl->fields) {
    a->accept(this);
  }

  for (auto &i : decl->inits) {
    i->accept(this);
  }

  if (auto [result, type] = Helper::checkImplementTraitSig(decl->symbol);
      !result) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S034);
    dia.labels = {{decl->span,
                   "type '" + decl->name + "' does not implement trait '" +
                       type->name + "'",
                   true}};
    engine.emit(dia);
    recover.recover();
  }
}
void Resolver::visit(EnumDecl *) {}
void Resolver::visit(ImplDecl *decl) {
  auto impl = table.registry.getImpl(decl);
  ScopeGuard _(table, impl->memberScope);
  TypeContextGuard __(currentType, impl->target);
  auto prev = currentSelf;
  currentSelf = impl->target->memberScope;
  for (auto &m : decl->LinkedImplMethods) {
    m->accept(this);
    if (!decl->traits.empty() &&
        !Helper::hasSameMethodSig(decl->sigs, m->methodSymbol).first) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S035);
      dia.labels = {
          {decl->span,
           "method '" + m->name + "' is not declared by the implemented traits",
           true}};
      dia.notes = {
          {"trait implementation blocks may only contain methods required by "
           "their traits"}};
      engine.emit(dia);
      recover.recover();
    }
  }

  currentSelf = prev;
}

void Resolver::visit(TraitDecl *decl) {
  ScopeGuard _(table, decl->symbol->memberScope);
  for (auto a : decl->traitSigs) {
    a->accept(this);
  }
}

void Resolver::visit(FuncDecl *decl) {

  ScopeGuard _(table, decl->methodSymbol->scope);
  auto symbol = decl->methodSymbol;
  auto prev = currentMethod;
  currentMethod = decl->methodSymbol;
  for (auto &p : decl->params) {
    p->accept(this);
  }

  decl->body->accept(this);

  auto rt = decl->returnType.get();
  rt->accept(this);
  auto type = rt->resolved;
  for (auto r : symbol->returns) {
    if (!isAssignable(type, r->returnType)) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S036);
      dia.labels = {{decl->span,
                     "expected return type '" + rt->type + "', found '" +
                         r->returnType->name + "'",
                     true}};
      engine.emit(dia);
      recover.recover();
    }
  }
  symbol->returnType = rt->resolved;

  currentMethod = prev;

  if (currentType->base != nullptr) {
    auto it = currentType->base->memberScope->methodMap.find(decl->name);

    if (it == currentType->base->memberScope->methodMap.end()) {
      if (decl->isOverride) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S037);
        dia.labels = {{decl->span, "no matching method to override", true}};
        dia.helps = {{"remove the override modifier or match a parent method "
                      "signature"}};
        engine.emit(dia);
        recover.recover();
      }
      return;
    }
    if (Helper::hasSameMethodSig(it->second, decl->methodSymbol).first) {
      if (!decl->isOverride) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S038);
        dia.labels = {{decl->span,
                       "inherited method overridden without 'override'", true}};
        dia.helps = {{"add the override modifier to this method"}};
        engine.emit(dia);
        recover.recover();
      }
    } else if (decl->isOverride) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S037);
      dia.labels = {{decl->span, "no matching method to override", true}};
      dia.helps = {{"remove the override modifier or match a parent method "
                    "signature"}};
      engine.emit(dia);
      recover.recover();
    }
  } else if (decl->isOverride) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S039);
    dia.labels = {{decl->span,
                   "'override' is not valid in a class without a parent",
                   true}};
    dia.helps = {{"remove the override modifier"}};
    engine.emit(dia);
    recover.recover();
  }

  if (decl->isFrame) {
    if (!dynamic_cast<MainSymbol *>(currentType)) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S040);
      dia.labels = {
          {decl->span, "frame method must be declared in 'Main'", true}};
      dia.helps = {{"change the method declaration to satisfy the frame method "
                    "requirements"}};
      engine.emit(dia);
      recover.recover();
    }
    if (decl->name != "update") {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S040);
      dia.labels = {{decl->span, "frame method's name must be update", true}};
      dia.helps = {{"change the method declaration to satisfy the frame method "
                    "requirements"}};
      engine.emit(dia);
      recover.recover();
    }
  }
}

static bool canFieldInit(Expr *init) {
  if (dynamic_cast<LiteralExpr *>(init)) {
    return true;
  }
  if (dynamic_cast<SpawnExpr *>(init)) {
    return true;
  }
  if (auto call = dynamic_cast<CallExpr *>(init)) {
    if (call->callType == CallExpr::CallType::INIT_CALL) {
      return true;
    }
    if (call->callType == CallExpr::CallType::PAYLOAD_CALL) {
      return true;
    }
  }
  if (auto member = dynamic_cast<MemberExpr *>(init)) {
    if (member->resolved->typeSymbol->kind == TypeSymbol::TypeKind::ENUM) {
      return true;
    }
  }

  // TODO: 배열 초기화 방식 추가시 관련 내용 추가하기.

  return false;
}

void Resolver::visit(VarDecl *decl) {

  if (!decl->type->resolved) {
    Error::internal(decl->span, "decl->type->resolved is nullptr");
  }

  if (!decl->symbol) {
    Error::internal(decl->span, "VarDecl symbol not initialized");
  }

  if (!decl->type || !decl->type->resolved) {
    Error::internal(decl->span, "VarDecl type not resolved");
  }

  if (!decl->symbol->typeSymbol) {
    Error::internal(decl->span, "VarDecl symbol type not initialized");
  }

  if (decl->isRoot && decl->init == nullptr) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S128);
    dia.labels = {
        {decl->span, "this root variable has no initializer", true},
    };
    dia.notes = {
        "root variables must have a known initial state before semantic "
        "verification",
    };
    dia.helps = {
        "add an initializer to this root variable declaration",
    };
    engine.emit(dia);
    recover.recover();
  }

  if (decl->init) {
    decl->init->accept(this);

    if (decl->symbol->typeSymbol->kind == TypeSymbol::TypeKind::CLASS) {
      if (!dynamic_cast<ViewExpr *>(decl->init.get())) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S041);
        dia.labels = {{decl->init->span,
                       "observer must be initialized from 'world.view'", true}};
        dia.helps = {{"initialize this observer with 'world.view(handle)'"}};
        engine.emit(dia);
        recover.recover();
      }
    }

    if (auto arr = dynamic_cast<ArrayTypeSymbol *>(decl->type->resolved)) {
      if (arr->baseType == decl->init->resolvedType) {
        if (auto g = dynamic_cast<GenericSymbol *>(arr->baseType)) {
          if (g->origin->kind == TypeSymbol::TypeKind::HANDLE) {
            auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S132);
            dia.labels = {
                {decl->init->span,
                 "this single spawn result is copied into every array element",
                 true},
            };
            dia.notes = {
                "all array elements will contain Handles to the same entity",
            };
            dia.helps = {
                "spawn each element separately if the array should contain "
                "distinct entities",
            };
            engine.emit(dia);
          }
        }
        return;
      }
    }

    if (auto lit = dynamic_cast<LiteralExpr *>(decl->init.get())) {
      convertLit(lit, decl->type.get());
      decl->symbol->typeSymbol = decl->type->resolved;
    } else {
      if (dynamic_cast<PrimtiveType *>(decl->type->resolved) &&
          !decl->type->setSize) {
        inferencePrim(decl->type.get(), decl->init->resolvedType);
        decl->symbol->typeSymbol = decl->type->resolved;
      }
      auto [result, kind] = Helper::canImplicitlyConvert(
          decl->init->resolvedType, decl->type->resolved);
      if (!result) {
        castFail(kind, decl->init->span);
      }
    }

    if (decl->context == DeclContext::CLASSBODY && decl->init) {
      if (!canFieldInit(decl->init.get())) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S041);
        dia.labels = {{decl->init->span,
                       "initializer is not valid for field type '" +
                           decl->type->type + "'",
                       true}};
        engine.emit(dia);
        recover.recover();
      }
    }
  }

  if (decl->symbol->typeSymbol->kind == TypeSymbol::TypeKind::CLASS) {
    if (decl->init == nullptr) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S043);
      dia.labels = {
          {decl->init->span, "observer requires an initializer", true}};
      engine.emit(dia);
      recover.recover();
    }
    if (currentMethod == nullptr) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S044);
      dia.labels = {
          {decl->init->span, "observer cannot be stored as a field", true}};
      engine.emit(dia);
      recover.recover();
    }
  }
}

void Resolver::visit(TypeNode *type) {
  TypeResolver::resolveTypeNode(type, typeContext);
}

void Resolver::visit(ASTNode *) {}

void Resolver::visit(TraitSig *sig) {

  for (auto p : sig->params) {
    p->accept(this);
    p->symbol->typeSymbol = p->type->resolved;
  }
}

static Expr *findInvalidDefaultValue(Expr *expr) {
  if (dynamic_cast<LiteralExpr *>(expr) ||
      dynamic_cast<DefaultValueExpr *>(expr)) {
    return nullptr;
  }

  auto *call = dynamic_cast<CallExpr *>(expr);
  if (!call || call->callType != CallExpr::CallType::INIT_CALL) {
    return expr;
  }

  for (const auto &arg : call->arguments) {
    if (Expr *invalid = findInvalidDefaultValue(arg.get())) {
      return invalid;
    }
  }

  return nullptr;
}
void Resolver::visit(Param *param) {
  param->type->accept(this);
  param->symbol->typeSymbol = param->type->resolved;
  if (!param->symbol->typeSymbol) {
    Error::internal(param->span, "param type is unlinked");
  }
  if (param->defaultValue.has_value()) {
    param->defaultValue.value()->accept(this);
    if (!isAssignable(param->type->resolved,
                      param->defaultValue.value()->resolvedType)) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S045);
      dia.labels = {{param->span,
                     "expected type '" + param->type->type + "', found '" +
                         param->defaultValue.value()->resolvedType->name + "'",
                     true}};
      engine.emit(dia);
      recover.recover();
    }

    auto expr = param->defaultValue.value().get();

    if (auto invaild = findInvalidDefaultValue(expr); invaild != nullptr) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S127);
      dia.labels = {
          {invaild->span, "this expression cannot be used as a default value",
           true},
      };
      dia.notes = {
          "default values are restricted to supported compile-time expression "
          "kinds",
      };
      dia.helps = {
          "use a literal or another supported default-value expression",
      };
      engine.emit(dia);
      recover.recover();
    }
  }
}

void Resolver::visit(InitDecl *decl) {
  auto symbol = decl->methodSymbol;
  for (auto r : symbol->returns) {
    if (r->returnType != table.registry.getBuilt("void")) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S046);
      dia.labels = {{decl->span, "init cannot declare a return type", true}};
      dia.helps = {{"remove the return type from this init declaration"}};
      engine.emit(dia);
      recover.recover();
    }
  }

  for (auto &p : decl->params) {
    p->accept(this);
  }

  auto prev = currentMethod;
  currentMethod = decl->methodSymbol;

  ScopeGuard _(table, decl->methodSymbol->scope);
  decl->body->accept(this);

  currentMethod = prev;
}

void Resolver::visit(OnDestroyDecl *decl) {
  auto symbol = decl->methodSymbol;
  for (auto r : symbol->returns) {
    if (r->returnType != table.registry.getBuilt("void")) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S047);
      dia.labels = {
          {decl->span, "onDestroy cannot declare a return type", true}};
      dia.helps = {{"remove the return type from this onDestroy declaration"}};
      engine.emit(dia);
      recover.recover();
    }
  }

  currentMethod = decl->methodSymbol;
  ScopeGuard _(table, decl->methodSymbol->scope);
  decl->body->accept(this);
}

void Resolver::visit(ImportDecl *) {}