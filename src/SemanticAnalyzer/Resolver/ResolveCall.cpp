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

    if (!expr->resolved) {
      Error::internal(expr->token, "unresolved function call target");
    }

    ResolveCall(expr);
    return;
  }

  // 2. receiver 해석
  expr->receiver->accept(this);

  auto *name = dynamic_cast<NameExpr *>(expr->receiver.get());
  if (!name) {
    Error::internal(expr->token, "call receiver must be a name expression");
  }

  // 3. 값 receiver -> 멤버 메서드 호출
  if (name->valueSymbol) {
    auto *ownerType = name->valueSymbol->typeSymbol;
    if (!ownerType) {
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

    auto it = scope->method.find(expr->methodName);
    if (it == scope->method.end()) {
      Error::diagnostic(expr->token,
                        "unknown method name: " + expr->methodName);
    }

    expr->callType = CallExpr::CallType::FUNC_CALL;
    expr->resolved = it->second.get();
    ResolveCall(expr);
    return;
  }

  // 4. 타입 receiver -> enum variant 혹은 static call
  if (name->typeSymbol) {
    if (name->typeSymbol->kind == TypeSymbol::TypeKind::ENUM) {
      auto it = name->typeSymbol->variantMap.find(expr->methodName);
      if (it == name->typeSymbol->variantMap.end()) {
        Error::diagnostic(expr->token,
                          "unknown variant name: " + expr->methodName);
      }

      expr->callType = CallExpr::CallType::PAYLOAD_CALL;
      expr->resolved = it->second;
      ResolveEnumVariant(expr);
      return;
    }

    // TODO: static method 지원 시 여기서 처리
    Error::diagnostic(expr->token, "static method is not supported yet: " +
                                       expr->methodName);
  }

  Error::internal(expr->token, "unresolved call receiver: " + name->token.text);
}