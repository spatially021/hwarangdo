#pragma once

#include "SemanticAnalyzer/ResolvedLit.h"
#include <variant>

class EnumVariantSymbol;

struct CaseKey {
  std::variant<ResolvedLit, EnumVariantSymbol *> value;

  explicit CaseKey(const ResolvedLit &lit) : value(lit) {}
  explicit CaseKey(EnumVariantSymbol *variant) : value(variant) {}

  bool operator==(const CaseKey &other) const { return value == other.value; }
};

inline size_t hashResolvedLit(const ResolvedLit &lit) {
  size_t valueHash = std::visit(
      [](const auto &v) -> size_t {
        using T = std::decay_t<decltype(v)>;

        if constexpr (std::is_same_v<T, bool>) {
          return std::hash<bool>{}(v);
        } else if constexpr (std::is_same_v<T, IntPayload>) {
          return llvm::hash_value(v.value);
        } else if constexpr (std::is_same_v<T, FloatPayload>) {
          return llvm::hash_value(v.value.bitcastToAPInt());
        } else if constexpr (std::is_same_v<T, CharPayload>) {
          return std::hash<uint32_t>{}(v.codePoint);
        } else if constexpr (std::is_same_v<T, StringPayload>) {
          return llvm::hash_combine_range(v.codePoints.begin(),
                                          v.codePoints.end());
        }
      },
      lit.value);

  return llvm::hash_combine(lit.type, lit.value.index(), valueHash);
}

struct CaseKeyHash {
  size_t operator()(const CaseKey &key) const {
    return std::visit(
        [](const auto &v) -> size_t {
          using T = std::decay_t<decltype(v)>;

          if constexpr (std::is_same_v<T, ResolvedLit>) {
            return hashResolvedLit(v);
          } else {
            return std::hash<const void *>{}(v);
          }
        },
        key.value);
  }
};
