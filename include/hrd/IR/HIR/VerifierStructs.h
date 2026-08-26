#include "hrd/IR/HIR/HIRSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
enum class FlowBaseKind : std::uint8_t {
  Local,
  Param,
  Self,
  Root,
};

enum class FlowProjectionKind : std::uint8_t {
  Field,
  ConstIndex,
  DynamicIndex,
};

struct FlowProjection {
  FlowProjectionKind kind = FlowProjectionKind::Field;

  // Field:
  //   HIRField*
  //
  // DynamicIndex:
  //   HIRValueExpr*
  const void *identity = nullptr;

  // ConstIndex에서 사용.
  std::string index;

  bool operator==(const FlowProjection &other) const {
    return kind == other.kind && identity == other.identity &&
           index == other.index;
  }
};

struct FlowKey {
  FlowBaseKind baseKind = FlowBaseKind::Local;
  const void *base = nullptr;

  std::vector<FlowProjection> projections;

  bool operator==(const FlowKey &other) const {
    return baseKind == other.baseKind && base == other.base &&
           projections == other.projections;
  }
};

struct FlowKeyHash {
  std::size_t operator()(const FlowKey &key) const noexcept {
    std::size_t seed =
        std::hash<std::uint8_t>{}(static_cast<std::uint8_t>(key.baseKind));

    auto combine = [&seed](std::size_t hash) {
      seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    };

    combine(std::hash<const void *>{}(key.base));

    for (const auto &projection : key.projections) {
      combine(std::hash<std::uint8_t>{}(
          static_cast<std::uint8_t>(projection.kind)));

      combine(std::hash<const void *>{}(projection.identity));
      combine(std::hash<std::string>{}(projection.index));
    }

    return seed;
  }
};

struct FlowState {
  InitState initial;
  InitState current;
};

using FlowMap = std::unordered_map<FlowKey, FlowState, FlowKeyHash>;
using FieldMap = std::unordered_map<ValueSymbol *, InitState>;

struct InitMap {
  std::unordered_map<HIRLocal *, InitState> localStates;
  std::unordered_map<HIRParam *, InitState> paramStates;
  FieldMap fieldStates;
  FieldMap rootStates;
  FlowMap flowStates;
};
