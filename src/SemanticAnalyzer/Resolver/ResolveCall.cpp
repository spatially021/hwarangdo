#include "hrd/AST/Decl.h"
#include "hrd/AST/Expr.h"
#include "hrd/AST/Stmt.h"
#include "hrd/SemanticAnalyzer/Resolver.h"
#include "hrd/SemanticAnalyzer/Scope.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/SourceSpan.h"
#include "hrd/util/Error.h"
#include <cstddef>
#include <string>
#include <variant>
#include <vector>

inline Expr::Ptr cloneExpr(const Expr::Ptr &expr) {
  return expr ? expr->deepCopy() : nullptr;
}

MethodSymbol *
Resolver::resolveMethodOverload(SourceSpan span,
                                const vector<MethodSymbol *> &bucket,
                                const vector<Expr *> &args) {
  return resolveOverload<MethodSymbol>(
      span, bucket, args, [](MethodSymbol *m) { return m->params.size(); },
      [](MethodSymbol *m, size_t i) { return m->params[i]->typeSymbol; },
      [](MethodSymbol *m, size_t i) {
        auto *func = dynamic_cast<FuncDecl *>(m->decl);
        if (!func)
          Error::internal("illegal ast kind");
        return func->params[i]->defaultValue.has_value();
      },
      "no matching method found", "ambiguous method call");
}

RuntimeSymbol *
Resolver::resolveRuntimeOverload(SourceSpan span,
                                 const vector<RuntimeSymbol *> &bucket,
                                 const vector<Expr *> &args) {
  return resolveOverload<RuntimeSymbol>(
      span, bucket, args, [](RuntimeSymbol *r) { return r->params.size(); },
      [](RuntimeSymbol *r, size_t i) { return r->params[i]; },
      [](RuntimeSymbol *, size_t) { return false; },
      "no matching runtime method found", "ambiguous runtime method call");
}

void Resolver::checkMethodAccess(CallExpr *expr, MethodSymbol *method,
                                 bool isImplicit) {
  if (isImplicit) {
    return;
  }

  auto isInternalReceiver = [&]() {
    if (expr->receiver == nullptr) {
      return true;
    }

    if (dynamic_cast<SelfExpr *>(expr->receiver.get())) {
      return true;
    }

    if (dynamic_cast<ThisExpr *>(expr->receiver.get())) {
      return true;
    }

    if (dynamic_cast<SuperExpr *>(expr->receiver.get())) {
      return true;
    }

    return false;
  };

  switch (method->modifier) {
  case AModifier::PUBLIC:
    return;

  case AModifier::PROTECTED:
    if (isInternalReceiver()) {
      return;
    }
    {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S019);
      dia.labels = {
          {expr->span, "protected method accessed here", true},
      };
      dia.notes = {{"protected methods can only be accessed from within the "
                    "declaring type or its derived types"}};
      engine.emit(dia);
      recover.recover();
    }
    return;

  case AModifier::PRIVATE:
    if (isInternalReceiver()) {
      return;
    }
    {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S020);
      dia.labels = {
          {expr->span, "private method accessed here", true},
      };
      dia.notes = {{"private methods can only be accessed from within the "
                    "declaring type"}};
      engine.emit(dia);
      recover.recover();
    }
    return;
  }
}

template <typename SymbolT, typename ParamCount, typename ParamType,
          typename HasDefault>
SymbolT *Resolver::resolveOverload(SourceSpan span,
                                   const std::vector<SymbolT *> &bucket,
                                   const std::vector<Expr *> &args,
                                   ParamCount paramCount, ParamType paramType,
                                   HasDefault hasDefault,
                                   std::string_view unknownMessage,
                                   std::string_view ambiguousMessage) {

  struct Candidate {
    std::vector<ArgMatchKind> kinds;
    SymbolT *symbol;
  };

  std::vector<Candidate> candidates;

  for (auto *sym : bucket) {
    if (paramCount(sym) != args.size()) {
      continue;
    }

    std::vector<ArgMatchKind> kinds;
    bool viable = true;

    for (size_t i = 0; i < args.size(); ++i) {
      auto kind = matchArgument(args[i], paramType(sym, i), hasDefault(sym, i));

      if (kind == ArgMatchKind::Invalid) {
        viable = false;
        break;
      }

      kinds.push_back(kind);
    }

    if (viable) {
      candidates.push_back({std::move(kinds), sym});
    }
  }

  std::vector<size_t> bestIdx;

  for (size_t i = 0; i < candidates.size(); ++i) {
    bool beaten = false;

    for (size_t j = 0; j < candidates.size(); ++j) {
      if (i == j) {
        continue;
      }

      if (isBetterThan(candidates[j].kinds, candidates[i].kinds)) {
        beaten = true;
        break;
      }
    }

    if (!beaten) {
      bestIdx.push_back(i);
    }
  }

  if (bestIdx.empty()) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S031);
    dia.labels = {
        {span, std::string(unknownMessage), true},
    };
    dia.notes = {
        {"the provided argument types do not match any available overload"}};
    engine.emit(dia);
    recover.recover();
  }

  if (bestIdx.size() > 1) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S032);
    dia.labels = {
        {span, std::string(ambiguousMessage), true},
    };
    dia.notes = {{"multiple overloads match the provided arguments"}};
    engine.emit(dia);
    recover.recover();
  }

  return candidates[bestIdx[0]].symbol;
}

void Resolver::resolveInit(CallExpr *expr) {
  auto *type = table.getType(expr->methodName);

  std::vector<Expr *> args;
  for (auto &a : expr->arguments) {
    a->accept(this);
    args.push_back(a.get());
  }

  auto &bucket = type->memberScope->inits;

  if (bucket.empty()) {
    if (args.empty()) {
      expr->resolvedType = type;
      return;
    }
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S021);
    dia.labels = {
        {expr->span, "cannot find a matching init declaration", true},
    };
    dia.notes = {{"no init declaration accepts the provided argument types"}};
    engine.emit(dia);
    recover.recover();
  }

  auto *best = resolveMethodOverload(expr->span, bucket, args);

  expr->resolved = best;
  expr->resolvedType = type;
}

void Resolver::ResolveEnumVariant(CallExpr *expr) {

  EnumVariantSymbol *variant = nullptr;
  if (auto e = get_if<EnumVariantSymbol *>(&expr->resolved)) {
    variant = *e;
  } else {
    Error::internal(expr->span, "illegal symbol kindi");
  }

  if (expr->arguments.size() > 1) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S022);
    dia.labels = {
        {expr->span, "only one payload is allowed", true},
    };
    dia.notes = {{"enum variants can contain at most one payload"}};
    engine.emit(dia);
    recover.recover();
  }

  if (expr->arguments.size() == 1) {
    auto *arg = expr->arguments[0].get();
    arg->accept(this);

    if (!arg->resolvedType) {
      Error::internal(arg->span, "unresolved payload type");
    }

    if (variant->payloadType == nullptr) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S023);
      dia.labels = {
          {expr->span, "this variant has no payload", true},
      };
      dia.notes = {{"this enum variant is declared without a payload"}};
      engine.emit(dia);
      recover.recover();
    }

    if (!isAssignable(variant->payloadType, arg->resolvedType)) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S024);
      dia.labels = {
          {expr->span, "expected payload of a different type", true},
      };
      dia.notes = {{"the payload type must match the type declared by the enum "
                    "variant"}};
      engine.emit(dia);
      recover.recover();
    }

    arg->resolvedType = implicitCasting(arg, variant->payloadType).first;
  } else {
    if (variant->payloadType != nullptr) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S025);
      dia.labels = {
          {expr->span, "missing payload", true},
      };
      dia.notes = {{"this enum variant must be constructed with a payload"}};
      engine.emit(dia);
      recover.recover();
    }
  }

  if (!expr->receiver || !expr->receiver->resolvedType) {
    Error::internal(expr->span, "unresolved enum receiver type");
  }

  // EnumName.Variant(...) 의 결과 타입은 enum 자체
  expr->resolvedType = expr->receiver->resolvedType;
}

void Resolver::resolveCall(CallExpr *expr, Scope *scope, bool isImplict) {
  auto bucketIt = scope->methodMap.find(expr->methodName);
  if (bucketIt == scope->methodMap.end()) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S027);
    dia.labels = {
        {expr->span, "unknown method called here", true},
    };
    dia.notes = {{"the target type does not declare a method with this name"}};
    engine.emit(dia);
    recover.recover();
  }

  std::vector<Expr *> args;
  for (auto &a : expr->arguments) {
    a->accept(this);
    args.push_back(a.get());
  }

  auto *best = resolveMethodOverload(expr->span, bucketIt->second, args);

  checkMethodAccess(expr, best, isImplict);

  expr->resolved = best;

  if (best->returnType == nullptr) {
    Error::internal(expr->span, "methodSymbol's returnType is nullptr");
  }

  expr->resolvedType = best->returnType;
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

ArgMatchKind Resolver::matchArgument(Expr *arg, TypeSymbol *param,
                                     bool hasInit) {

  if (dynamic_cast<DefaultValueExpr *>(arg)) {
    if (hasInit) {
      return ArgMatchKind::DefaultArg;
    } else {
      return ArgMatchKind::Invalid;
    }
  }

  if (arg->resolvedType == param) {
    return ArgMatchKind::Exact;
  }

  if (auto lit = dynamic_cast<LiteralExpr *>(arg)) {
    if (canImplicitlyLiteralConvert(lit, param).first) {
      return ArgMatchKind::ImplicitCast;
    }

  } else {
    if (canImplicitlyConvert(arg->resolvedType, param).first) {
      return ArgMatchKind::ImplicitCast;
    }
  }

  return ArgMatchKind::Invalid;
}
bool Resolver::tryResolveRuntime(CallExpr *expr) {
  if (expr->receiver == nullptr) {
    return false;
  }

  auto *name = dynamic_cast<NameExpr *>(expr->receiver.get());
  if (name == nullptr) {
    return false;
  }

  auto ns = table.runtimeMap.find(name->name);
  if (ns == table.runtimeMap.end()) {
    return false;
  }

  auto it = ns->second.functions.find(expr->methodName);
  if (it == ns->second.functions.end()) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S026);
    dia.labels = {
        {expr->span, "unknown runtime method called here", true},
    };
    engine.emit(dia);
    recover.recover();
  }

  std::vector<Expr *> args;
  for (auto &a : expr->arguments) {
    a->accept(this);
    args.push_back(a.get());
  }

  auto *best = resolveRuntimeOverload(expr->span, it->second, args);

  expr->resolved = best;
  expr->resolvedType = best->returnType;

  return true;
}
void Resolver::visit(CallExpr *expr) {
  if (expr->callType != CallExpr::CallType::UNRESOLVED) {
    return;
  }

  if (tryResolveRuntime(expr)) {
    expr->callType = CallExpr::CallType::RUNTIME_CALL;
    return;
  }

  // 1. receiver 없는 호출
  if (expr->receiver == nullptr) {

    if (table.isType(expr->methodName)) {
      expr->callType = CallExpr::CallType::INIT_CALL;
      resolveInit(expr);
      return;
    }

    auto it = currentType->memberScope->methodMap.find(expr->methodName);
    if (it != currentType->memberScope->methodMap.end()) {
      expr->callType = CallExpr::CallType::FUNC_CALL;
      resolveCall(expr, currentType->memberScope, true);
      return;
    }

    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S027);
    dia.labels = {
        {expr->span, "unknown method called here", true},
    };
    dia.notes = {{"the target type does not declare a method with this name"}};
    engine.emit(dia);
    recover.recover();
  }

  // 2. receiver 해석
  expr->receiver->accept(this);
  if (isTypeReceiver(expr->receiver.get())) {
    if (expr->receiver->resolvedType->kind == TypeSymbol::TypeKind::ENUM) {
      auto it = expr->receiver->resolvedType->variantMap.find(expr->methodName);
      if (it == expr->receiver->resolvedType->variantMap.end()) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S028);
        dia.labels = {
            {expr->span, "unknown variant referenced here", true},
        };
        dia.notes = {
            {"the target enum does not declare a variant with this name"}};
        engine.emit(dia);
        recover.recover();
      }

      expr->callType = CallExpr::CallType::PAYLOAD_CALL;
      expr->resolved = it->second;
      ResolveEnumVariant(expr);
      return;
    } else {
      // TODO:정적 메서드 추가시 추가.
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S029);
      dia.labels = {
          {expr->span, "static methods are not supported yet", true},
      };
      dia.notes = {{"support for static methods has not been implemented yet"}};
      engine.emit(dia);
      recover.recover();
    }
  } else {
    auto *ownerType = expr->receiver->resolvedType;
    if (ownerType == nullptr) {
      Error::internal(expr->span, "unresolved type");
    }

    if (auto g = dynamic_cast<GenericSymbol *>(ownerType)) {
      if (g->origin == table.getHandle()) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S030);
        dia.labels = {
            {expr->span, "cannot access member of handle type", true},
        };
        dia.notes = {
            {"dereference or resolve the handle before accessing members"}};
        engine.emit(dia);
        recover.recover();
      }
    }

    auto *scope = ownerType->memberScope;
    if (!scope) {
      Error::internal(expr->span, "memberScope is nullptr: " + ownerType->name);
    }

    expr->callType = CallExpr::CallType::FUNC_CALL;
    resolveCall(expr, scope);
    return;
  }

  Error::internal(expr->span, "unresolved call receiver");
}