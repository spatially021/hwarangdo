#include "hrd/AST/Expr.h"
#include "hrd/Imported/ImportedSymbolBuilder.h"
#include "hrd/MetaData/MetaData.h"
#include "hrd/SourceSpan.h"
#include "hrd/Token.h"
#include "hrd/util/Error.h"
#include "hrd/util/Helper.h"
#include <memory>
#include <variant>

void ImportedSymbolBuilder::resolveMethod(MethodMeta &ref) {
  auto *symbol = getMethod(ref);

  symbol->returnType = getOrCreateTypeRef(ref.returnType);

  for (size_t i = 0; i < symbol->params.size(); ++i) {
    if (!ref.params[i].defaultValue.has_value()) {
      continue;
    }

    auto *pSymbol = symbol->params[i];
    auto &value = ref.params[i].defaultValue.value();

    auto result = resolveDefault(value);

    if (!result) {
      Error::internal("failed to resolve default value");
    }

    if (auto call = dynamic_cast<CallExpr *>(result.get())) {
      pSymbol->defaultValue = call;
      continue;
    }
    if (auto lit = dynamic_cast<LiteralExpr *>(result.get())) {
      pSymbol->defaultValue = lit;
      continue;
    }
    Error::internal("failed to resolve default value");
  }
}

Expr::Ptr ImportedSymbolBuilder::resolveDefault(DefaultValueMeta &ref) {
  if (ref.kind == DefaultValueKind::Literal) {
    auto lit = make_shared<LiteralExpr>(SourceSpan(), Token(), "imported-lit");

    if (!ref.literal.has_value()) {
      Error::internal("default kind is lit but literal is nullopt");
    }

    lit->resolvedLit = ref.literal.value();
    lit->resolvedType = getOrCreateTypeRef(ref.resolvedType);

    ast.push_back(lit);

    return lit;
  }

  if (ref.kind == DefaultValueKind::StructInit) {
    if (!ref.type.has_value()) {
      Error::internal("default kind is init but type is nullopt");
    }

    TypeSymbol *type = getOrCreateTypeRef(ref.type.value());

    if (type == nullptr) {
      Error::internal("failed to resolve default init type");
    }

    std::vector<Expr::Ptr> args;
    args.reserve(ref.args.size());

    for (auto &arg : ref.args) {
      auto expr = resolveDefault(arg);

      if (!expr) {
        Error::internal("failed to resolve default init argument");
      }

      args.push_back(std::move(expr));
    }

    MethodSymbol *init = resolveInit(type, ref.args);

    auto call =
        make_shared<CallExpr>(SourceSpan(), nullptr, "init", std::move(args));

    call->resolvedType = getOrCreateTypeRef(ref.resolvedType);
    call->resolved = init;

    ast.push_back(call);

    return call;
  }

  Error::internal("unknown DefaultValueKind");
}

static int rankOf(const ArgMatchKind &kind) {
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

static bool isBetterThan(const vector<ArgMatchKind> &a,
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

MethodSymbol *
ImportedSymbolBuilder::resolveInit(TypeSymbol *type,
                                   std::vector<DefaultValueMeta> &args) {
  if (type == nullptr) {
    Error::internal("resolveInit: type is nullptr");
  }

  if (type->memberScope == nullptr) {
    Error::internal("resolveInit: type has no member scope");
  }

  auto &bucket = type->memberScope->inits;

  // init이 하나도 없고 인자도 없다면 implicit default init
  if (bucket.empty()) {
    if (args.empty()) {
      return nullptr;
    }

    Error::internal("resolveInit: no matching init");
  }

  struct Candidate {
    std::vector<ArgMatchKind> kinds;
    MethodSymbol *symbol;
  };

  std::vector<Candidate> candidates;

  for (auto *init : bucket) {
    if (init == nullptr) {
      Error::internal("resolveInit: null init symbol");
    }

    if (init->params.size() != args.size()) {
      continue;
    }

    std::vector<ArgMatchKind> kinds;
    bool viable = true;

    for (size_t i = 0; i < args.size(); ++i) {
      auto *argType = getOrCreateTypeRef(args[i].resolvedType);
      auto *paramType = init->params[i]->typeSymbol;

      if (argType == nullptr || paramType == nullptr) {
        Error::internal("resolveInit: unresolved argument or parameter type");
      }

      auto kind = matchArgument(argType, paramType);

      if (kind == ArgMatchKind::Invalid) {
        viable = false;
        break;
      }

      kinds.push_back(kind);
    }

    if (viable) {
      candidates.push_back({std::move(kinds), init});
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
    Error::internal("resolveInit: no matching init");
  }

  if (bestIdx.size() > 1) {
    Error::internal("resolveInit: ambiguous init");
  }

  return candidates[bestIdx[0]].symbol;
}

ArgMatchKind ImportedSymbolBuilder::matchArgument(TypeSymbol *from,
                                                  TypeSymbol *to) {
  if (from == nullptr || to == nullptr) {
    return ArgMatchKind::Invalid;
  }

  if (from == to) {
    return ArgMatchKind::Exact;
  }

  auto [result, kind] = Helper::canImplicitlyConvert(from, to);

  if (!result) {
    return ArgMatchKind::Invalid;
  }

  switch (kind) {
  case CastingResultKind::None:
    return ArgMatchKind::Exact;
  default:
    return ArgMatchKind::ImplicitCast;
  }
}