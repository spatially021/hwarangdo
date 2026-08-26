#pragma once

#include "hrd/BuiltInType.h"
#include "hrd/Inputs.h"
#include "hrd/SemanticAnalyzer/SymbolTable/Hash.h"
#include <llvm/ADT/APInt.h>
#include <optional>
#include <string>
#include <vector>

enum class TypeRefKind {
  BuiltIn,
  Declared,
  Generic,
  Array,
};

struct TypeRef {
  TypeRefKind kind;

  // Declared / Generic
  std::string name;
  SourcePath path;

  // BuiltIn
  std::optional<BuiltInType> builtIn;

  // Generic: generic arguments
  // Array: args[0] == element type
  std::vector<TypeRef> args;

  // Array only
  std::optional<llvm::APInt> arraySize;

  bool operator==(const TypeRef &other) const {
    return kind == other.kind && name == other.name && path == other.path &&
           builtIn == other.builtIn && args == other.args &&
           arraySize == other.arraySize;
  }
};

struct TypeRefHash {
  std::size_t operator()(const TypeRef &type) const noexcept {
    std::size_t seed = 0;

    auto combine = [&seed](std::size_t hash) {
      seed ^= hash + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    };

    combine(std::hash<TypeRefKind>{}(type.kind));
    combine(std::hash<std::string>{}(type.name));
    combine(PathHash{}(type.path));

    if (type.builtIn) {
      combine(std::hash<BuiltInType>{}(*type.builtIn));
    }

    for (const auto &arg : type.args) {
      combine(TypeRefHash{}(arg));
    }

    if (type.arraySize) {
      const llvm::APInt &value = *type.arraySize;

      combine(std::hash<unsigned>{}(value.getBitWidth()));

      for (unsigned i = 0; i < value.getNumWords(); ++i) {
        combine(std::hash<uint64_t>{}(value.getRawData()[i]));
      }
    }

    return seed;
  }
};