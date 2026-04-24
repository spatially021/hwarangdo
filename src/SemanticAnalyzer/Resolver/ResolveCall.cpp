#include "AST/Decl.h"
#include "AST/Expr.h"
#include "SemanticAnalyzer/Resolver.h"
#include "SemanticAnalyzer/Scope.h"
#include "SemanticAnalyzer/symbol/MethodSymbol.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "SemanticAnalyzer/symbol/ValueSymbol.h"
#include "util/Error.h"
#include <cstddef>
#include <vector>

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

void Resolver::ResolveCall(CallExpr *expr, Scope *scope) {
  auto &bucket = scope->methodMap[expr->methodName];
  vector<TypeSymbol *> args; // nullptr -> defaultValue
  for (auto &a : expr->arguments) {
    a->accept(this);
    args.push_back(a->resolvedType);
  }
  vector<pair<vector<ArgMatchKind>, MethodSymbol *>> candidates;
  for (auto &m : bucket) {
    if (m->paramTypes.size() != args.size()) {
      continue;
    }

    vector<ArgMatchKind> kinds;
    bool viable = true;

    auto func = dynamic_cast<FuncDecl *>(m->decl);
    if (func == nullptr) {
      Error::internal(expr->token, "illegal ast kind");
    }

    for (size_t i = 0; i < m->paramTypes.size(); ++i) {
      auto kind = matchArgument(args[i], m->paramTypes[i],
                                func->params[i]->defaultValue.has_value());
      if (kind == ArgMatchKind::Invalid) {
        viable = false;
        break;
      }
      kinds.push_back(kind);
    }

    if (viable) {
      candidates.push_back({std::move(kinds), m});
    }
  }
  vector<size_t> bestIdx;
  for (size_t i = 0; i < candidates.size(); ++i) {
    bool beaten = false;
    for (size_t j = 0; j < candidates.size(); ++j) {
      if (i == j)
        continue;
      if (isBetterThan(candidates[j].first, candidates[i].first)) {
        beaten = true;
        break;
      }
    }
    if (!beaten) {
      bestIdx.push_back(i);
    }
  }

  if (bestIdx.empty()) {
    Error::diagnostic(expr->token, "unknown call");
  }

  if (bestIdx.size() == 1) {
    auto best = candidates[bestIdx[0]].second;
    expr->resolved = best;
    expr->resolvedType = best->returnType;
    return;
  }

  Error::diagnostic(expr->token, "ambiguous overload call");
}
int Resolver::rankOf(const ArgMatchKind &kind) {
  switch (kind) {
  case ArgMatchKind::Exact:
  case ArgMatchKind::DefaultArg:
    return 0;
  case ArgMatchKind::ImplicitCast:
    return 1;
  case ArgMatchKind::Invalid:
    return 999;
  }
  return 999;
}

bool Resolver::isBetterThan(const vector<ArgMatchKind> &a,
                            const vector<ArgMatchKind> &b) {
  bool better = false;
  for (size_t i = 0; i < a.size(); ++i) {
    int ar = rankOf(a[i]);
    int br = rankOf(b[i]);
    if (ar > br) {
      return false; // 한 위치라도 더 나쁘면 우세 아님
    }
    if (ar < br) {
      better = true;
    }
  }

  return better;
}

ArgMatchKind Resolver::matchArgument(TypeSymbol *arg, TypeSymbol *param,
                                     bool hasInit) {
  if (arg == table->getDefaultV()) {
    if (hasInit) {
      return ArgMatchKind::DefaultArg;
    } else {
      return ArgMatchKind::Invalid;
    }
  }

  if (arg == param) {
    return ArgMatchKind::Exact;
  }

  if (canImplicitlyConvert(arg, param)) {
    return ArgMatchKind::ImplicitCast;
  }

  return ArgMatchKind::Invalid;
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
    ResolveCall(expr, currentType->memberScope);
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

    expr->callType = CallExpr::CallType::FUNC_CALL;
    ResolveCall(expr, scope);
    return;
  }

  Error::internal(expr->token,
                  "unresolved call receiver: " + expr->receiver->token.text);
}