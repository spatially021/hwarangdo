#pragma once
#include "hrd/MetaData/MetaData.h"
struct FieldHash {
  std::size_t operator()(const FieldMeta &field) const noexcept {
    std::size_t seed = 0;

    auto combine = [&seed](std::size_t hash) {
      seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    };

    combine(std::hash<std::string>{}(field.name));
    combine(TypeRefHash{}(field.type));
    combine(std::hash<AModifier>{}(field.modifier));

    return seed;
  }
};

struct ParamHash {
  std::size_t operator()(const ParamMeta &param) const noexcept {
    return TypeRefHash{}(param.type);
  }
};

struct MethodHash {
  std::size_t operator()(const MethodMeta &method) const noexcept {
    std::size_t seed = 0;

    auto combine = [&seed](std::size_t hash) {
      seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    };

    combine(std::hash<std::string>{}(method.name));
    combine(TypeRefHash{}(method.returnType));

    for (const auto &param : method.params) {
      combine(ParamHash{}(param));
    }

    combine(std::hash<AModifier>{}(method.modifier));

    return seed;
  }
};

struct EnumVariantHash {
  std::size_t operator()(const EnumVariantMeta &variant) const noexcept {
    std::size_t seed = 0;

    auto combine = [&seed](std::size_t hash) {
      seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    };

    combine(std::hash<std::string>{}(variant.name));

    if (variant.payload.has_value()) {
      combine(TypeRefHash{}(*variant.payload));
    } else {
      combine(0);
    }

    return seed;
  }
};

struct TypeMetaHash {
  std::size_t operator()(const TypeMeta &type) const noexcept {
    std::size_t seed = 0;

    auto combine = [&seed](std::size_t hash) {
      seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    };

    combine(std::hash<std::string>{}(type.name));
    combine(std::hash<TypeKind>{}(type.kind));
    combine(PathHash{}(type.path));

    for (const auto &field : type.fields) {
      combine(FieldHash{}(field));
    }

    for (const auto &method : type.methods) {
      combine(MethodHash{}(method));
    }

    for (const auto &variant : type.variants) {
      combine(EnumVariantHash{}(variant));
    }

    if (type.parent.has_value()) {
      combine(TypeRefHash{}(*type.parent));
    } else {
      combine(0);
    }

    for (const auto &trait : type.traits) {
      combine(TypeRefHash{}(trait));
    }

    return seed;
  }
};