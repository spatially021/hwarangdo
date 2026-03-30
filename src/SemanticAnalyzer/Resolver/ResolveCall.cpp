#include "AST/Decl.h"
#include "AST/Expr.h"
#include "SemanticAnalyzer/Resolver.h"
#include "SemanticAnalyzer/Scope.h"
#include "SemanticAnalyzer/symbol/MethodSymbol.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "SemanticAnalyzer/symbol/ValueSymbol.h"
#include "util/Error.h"

inline Expr::Ptr cloneExpr(const Expr::Ptr &expr) {
  return expr ? expr->deepCopy() : nullptr;
}

namespace {

// resolved가 특정 타입 심볼인지 강하게 확인
template <typename T>
T *checkedSymbolCast(Symbol *symbol, const Token &token,
                     const std::string &msg) {
  auto *result = dynamic_cast<T *>(symbol);
  if (!result) {
    Error::internal(token, msg);
  }
  return result;
}

} // namespace

void Resolver::ResolveEnumVariant(CallExpr *expr) {
  auto *variant = checkedSymbolCast<EnumVariantSymbol>(
      expr->resolved, expr->token, "expected enum variant symbol");

  if (expr->arguments.size() > 1) {
    Error::diagnostic(expr->token, "enum variant allows at most one payload");
  }

  if (expr->arguments.size() == 1) {
    auto *arg = expr->arguments[0].get();
    arg->accept(this);

    if (!arg->resolvedType) {
      Error::internal(arg->token, "unresolved payload type");
    }

    if (variant->payloadType == nullptr) {
      Error::diagnostic(expr->token, "this variant does not take payload: " +
                                         expr->methodName);
    }

    if (!isAssignable(variant->payloadType, arg->resolvedType)) {
      Error::diagnostic(expr->token, "incorrect payload type");
    }

    arg->resolvedType =
        implicitCasting(arg->resolvedType, variant->payloadType);
  } else {
    if (variant->payloadType != nullptr) {
      Error::diagnostic(expr->token, expr->methodName + " needs payload");
    }
  }

  if (!expr->receiver || !expr->receiver->resolvedType) {
    Error::internal(expr->token, "unresolved enum receiver type");
  }

  // EnumName.Variant(...) 의 결과 타입은 enum 자체
  expr->resolvedType = expr->receiver->resolvedType;
}

void Resolver::ResolveCall(CallExpr *expr) {
  auto *method = checkedSymbolCast<MethodSymbol>(expr->resolved, expr->token,
                                                 "expected method symbol");

  auto *funcDecl = dynamic_cast<FuncDecl *>(method->decl);
  if (!funcDecl) {
    Error::internal(expr->token, "method decl is not FuncDecl");
  }

  if (expr->arguments.size() != funcDecl->params.size()) {
    Error::diagnostic(expr->token, "mismatch argument count");
  }

  for (size_t i = 0; i < expr->arguments.size(); ++i) {
    auto *arg = expr->arguments[i].get();
    arg->accept(this);

    if (!arg->resolvedType) {
      Error::internal(arg->token, "unresolved argument type");
    }

    auto *paramType = funcDecl->params[i]->symbol->typeSymbol;
    if (!paramType) {
      Error::internal(funcDecl->params[i]->token, "unresolved parameter type");
    }

    if (arg->resolvedType == table->getDefaultV()) {
      auto &param = funcDecl->params[i];
      if (!param->defaultValue.has_value()) {
        Error::diagnostic(expr->token,
                          "this argument has no default value: " + param->name);
      }

      auto defaultExpr = cloneExpr(*param->defaultValue);
      defaultExpr->accept(this);

      if (!canImplicitlyConvert(defaultExpr->resolvedType, paramType)) {
        Error::diagnostic(expr->token,
                          "default value type does not match parameter type");
      }

      defaultExpr->resolvedType =
          implicitCasting(defaultExpr->resolvedType, paramType);

      expr->arguments[i] = defaultExpr;
    } else {
      if (!canImplicitlyConvert(arg->resolvedType, paramType)) {
        Error::diagnostic(expr->token, "unmatched argument type");
      }

      arg->resolvedType = implicitCasting(arg->resolvedType, paramType);
    }
  }

  if (!method->returnType) {
    Error::internal(expr->token, "unresolved return type");
  }

  // 중요: resolved는 계속 callee(MethodSymbol)로 유지
  expr->resolvedType = method->returnType;
}

void Resolver::visit(CallExpr *expr) {
  if (expr->callType != CallExpr::CallType::UNRESOLVED) {
    return;
  }

  // 1. receiver 없는 호출
  if (expr->receiver == nullptr) {
    expr->callType = CallExpr::CallType::FUNC_CALL;
    auto it = currentType->memberScope->methodMap.find(expr->methodName);
    if (it == currentType->memberScope->methodMap.end()) {
      Error::diagnostic(expr->token,
                        "cannot find method name : " + expr->methodName);
    }
    expr->resolved = it->second.get();
    ResolveCall(expr);
    return;
  }

  // 2. receiver 해석
  expr->receiver->accept(this);
  if (isTypeReceiver(expr->receiver.get())) {
    if (expr->receiver->resolvedType->kind == TypeSymbol::TypeKind::ENUM) {
      auto it = expr->receiver->resolvedType->variantMap.find(expr->methodName);
      if (it == expr->receiver->resolvedType->variantMap.end()) {
        Error::diagnostic(expr->token,
                          "unknown variant name: " + expr->methodName);
      }

      expr->callType = CallExpr::CallType::PAYLOAD_CALL;
      expr->resolved = it->second;
      ResolveEnumVariant(expr);
      return;
    } else {
      // TODO:정적 메서드 추가시 추가.
      Error::diagnostic(expr->token, "static method is not supported yet");
    }
    Error::diagnostic(expr->token, "static method is not supported yet: " +
                                       expr->methodName);
  } else {
    auto *ownerType = expr->receiver->resolvedType;
    if (ownerType == nullptr) {
      Error::internal(expr->token,
                      "unresolved type: " + expr->receiver->token.text);
    }

    if (auto g = dynamic_cast<GenericSymbol *>(ownerType)) {
      if (g->origin == table->getHandle()) {
        Error::diagnostic(expr->token, "handle type cannot access member : " +
                                           g->args[0]->name);
      }
    }

    auto *scope = ownerType->memberScope;
    if (!scope) {
      Error::internal(expr->token,
                      "memberScope is nullptr: " + ownerType->name);
    }

    auto it = scope->methodMap.find(expr->methodName);
    if (it == scope->methodMap.end()) {
      Error::diagnostic(expr->token,
                        "unknown method name: " + expr->methodName);
    }

    expr->callType = CallExpr::CallType::FUNC_CALL;
    expr->resolved = it->second.get();
    ResolveCall(expr);
    return;
  }

  Error::internal(expr->token,
                  "unresolved call receiver: " + expr->receiver->token.text);
}