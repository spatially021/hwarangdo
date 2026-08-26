#pragma once

#include "hrd/SemanticAnalyzer/SymbolTable/Key.h"

struct ArrayTypeHash {
  size_t operator()(const ArrayTypeKey &k) const {
    using llvm::hash_value;
    return llvm::hash_combine(k.base, hash_value(k.size));
  }
};

struct GenericInsHash {
  size_t operator()(const GenericInsKey &k) const {
    size_t h = std::hash<TypeSymbol *>()(k.origin);

    for (auto *arg : k.args) {
      h ^= std::hash<TypeSymbol *>()(arg) + 0x9e3779b9 + (h << 6) + (h >> 2);
    }

    return h;
  }
};

struct PathHash {
  std::size_t operator()(const SourcePath &path) const noexcept {
    std::size_t seed = 0;
    auto p = path.segments;
    for (const auto &segment : p) {
      const std::size_t value = std::hash<std::string>{}(segment);

      seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    }

    return seed;
  }
};