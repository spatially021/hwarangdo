#pragma once

#include "hrd/Inputs.h"
#include "hrd/SemanticAnalyzer/ResolvedLit.h"

#include "hrd/MetaData/TypeRef.h"
#include "hrd/enums/AccessModifier.h"

#include <llvm/ADT/APInt.h>
#include <optional>
#include <string>
#include <vector>

struct FieldMeta;
struct MethodMeta;
struct EnumVariantMeta;

enum class TypeKind {
  Enum,
  Class,
  Struct,
};
struct TypeMeta {
  std::string name;
  TypeKind kind;
  SourcePath path;
  std::vector<FieldMeta> fields;
  std::vector<MethodMeta> methods;
  std::vector<EnumVariantMeta> variants;

  std::optional<TypeRef> parent;
  std::vector<TypeRef> traits;

  bool operator==(const TypeMeta &rhs) const {
    return name == rhs.name && kind == rhs.kind && path == rhs.path &&
           fields == rhs.fields && methods == rhs.methods &&
           variants == rhs.variants && parent == rhs.parent &&
           traits == rhs.traits;
  }
};

struct TraitMeta {
  std::string name;
  SourcePath path;
  std::vector<MethodMeta> methods;

  bool operator==(const TraitMeta &rhs) const {
    return name == rhs.name && path == rhs.path && methods == rhs.methods;
  }
};

struct FieldMeta {
  std::string name;
  TypeRef type;
  AModifier modifier;

  bool operator==(const FieldMeta &rhs) const {
    return name == rhs.name && type == rhs.type && modifier == rhs.modifier;
  }
};

enum class DefaultValueKind {
  Literal,
  StructInit,
};

struct DefaultValueMeta {
  DefaultValueKind kind;
  TypeRef resolvedType;
  std::optional<ResolvedLit> literal;

  std::optional<TypeRef> type;
  std::vector<DefaultValueMeta> args;

  bool operator==(const DefaultValueMeta &rhs) const {
    return kind == rhs.kind && resolvedType == rhs.resolvedType &&
           literal == rhs.literal && type == rhs.type && args == rhs.args;
  }
};

struct ParamMeta {
  std::string name = "";
  TypeRef type;
  std::optional<DefaultValueMeta> defaultValue;

  bool operator==(const ParamMeta &rhs) const {
    return name == rhs.name && type == rhs.type &&
           defaultValue == rhs.defaultValue;
  }
};

struct MethodMeta {
  std::string name;
  TypeRef returnType;
  std::vector<ParamMeta> params;
  AModifier modifier;

  bool operator==(const MethodMeta &rhs) const {
    return name == rhs.name && returnType == rhs.returnType &&
           params == rhs.params && modifier == rhs.modifier;
  }
};

struct EnumVariantMeta {
  std::string name;
  std::optional<TypeRef> payload;

  bool operator==(const EnumVariantMeta &rhs) const {
    return name == rhs.name && payload == rhs.payload;
  }
};

struct ModuleMeta {
  std::vector<TypeMeta> types;
  std::vector<TraitMeta> traits;

  bool operator==(const ModuleMeta &rhs) const {
    return types == rhs.types && traits == rhs.traits;
  }
};