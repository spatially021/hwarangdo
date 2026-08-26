#pragma once

#include "hrd/MetaData/MetaData.h"

#include <iosfwd>
#include <string_view>

class MetaWriter {

public:
  MetaWriter(bool rewrite = false);

  void write(std::string_view moduleName, const ModuleMeta &meta,
             std::ostream &out);

private:
  std::ostream *out = nullptr;
  unsigned indentLevel = 0;
  bool rewrite;
  std::ostream &stream();

  // declaration
  void writeType(const TypeMeta &meta);
  void writeTrait(const TraitMeta &meta);

  void writeField(const FieldMeta &meta);
  void writeMethod(const MethodMeta &meta);
  void writeVariant(const EnumVariantMeta &meta);
  void writeParam(const ParamMeta &meta);

  // recursive values
  void writeTypeRef(const TypeRef &ref);
  void writeDefaultValue(const DefaultValueMeta &value);
  void writeLiteral(const ResolvedLit &lit);

  // path / formatting
  void writePath(const SourcePath &path);
  void writeQualifiedName(const SourcePath &path, std::string_view name);

  void writeIndent();

  static std::string_view typeKindName(TypeKind kind);
  static std::string_view modifierName(AModifier modifier);
  static std::string_view builtInName(BuiltInType type);

  static bool isSignedInteger(BuiltInType type);

  static void writeEscapedChar(std::ostream &out, uint32_t codePoint);
  static void writeEscapedString(std::ostream &out,
                                 const std::vector<uint32_t> &codePoints);
};