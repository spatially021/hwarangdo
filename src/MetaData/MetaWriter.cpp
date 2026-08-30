#include "hrd/MetaData/MetaWriter.h"

#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/util/Error.h"

#include <llvm/ADT/SmallString.h>

#include <ostream>
#include <string>

MetaWriter::MetaWriter(bool b) : rewrite(b) {}

// ============================================================
// Public
// ============================================================

void MetaWriter::write(std::string_view moduleName, const ModuleMeta &meta,
                       std::ostream &output) {
  if (!output.good()) {
    Error::internal("invalid output stream passed to MetaWriter");
  }

  out = &output;
  indentLevel = 0;

  stream() << "module " << moduleName << ";\n";

  if (!meta.types.empty() || !meta.traits.empty()) {
    stream() << '\n';
  }

  bool first = true;

  for (const TypeMeta &type : meta.types) {
    if (!first) {
      stream() << '\n';
    }

    writeType(type);
    first = false;
  }

  for (const TraitMeta &trait : meta.traits) {
    if (!first) {
      stream() << '\n';
    }

    writeTrait(trait);
    first = false;
  }

  if (!output.good()) {
    out = nullptr;
    Error::internal("failed while writing metadata");
  }

  out = nullptr;
}

std::ostream &MetaWriter::stream() {
  if (out == nullptr) {
    Error::internal("MetaWriter output stream is not initialized");
  }

  return *out;
}

// ============================================================
// Type
// ============================================================

void MetaWriter::writeType(const TypeMeta &meta) {
  writeIndent();

  stream() << typeKindName(meta.kind) << ' ';
  writeQualifiedName(meta.path, meta.name);

  if (meta.parent.has_value()) {
    stream() << " : ";
    writeTypeRef(*meta.parent);
  }

  stream() << " {\n";

  ++indentLevel;

  bool wroteMember = false;

  for (const FieldMeta &field : meta.fields) {
    writeField(field);
    wroteMember = true;
  }

  if (!meta.fields.empty() &&
      (!meta.methods.empty() || !meta.variants.empty())) {
    stream() << '\n';
  }

  for (const MethodMeta &method : meta.methods) {
    writeMethod(method);
    wroteMember = true;
  }

  if (!meta.methods.empty() && !meta.variants.empty()) {
    stream() << '\n';
  }

  for (const EnumVariantMeta &variant : meta.variants) {
    writeVariant(variant);
    wroteMember = true;
  }

  (void)wroteMember;

  --indentLevel;

  writeIndent();
  stream() << "}\n";
}

// ============================================================
// Trait
// ============================================================

void MetaWriter::writeTrait(const TraitMeta &meta) {
  writeIndent();

  stream() << "trait ";
  writeQualifiedName(meta.path, meta.name);

  stream() << " {\n";

  ++indentLevel;

  for (const MethodMeta &method : meta.methods) {
    writeMethod(method);
  }

  --indentLevel;

  writeIndent();
  stream() << "}\n";
}

// ============================================================
// Field
// ============================================================

void MetaWriter::writeField(const FieldMeta &meta) {
  writeIndent();

  stream() << modifierName(meta.modifier) << " field " << meta.name << ": ";

  writeTypeRef(meta.type);

  stream() << ";\n";
}

// ============================================================
// Method
// ============================================================

void MetaWriter::writeMethod(const MethodMeta &meta) {
  writeIndent();

  stream() << modifierName(meta.modifier) << " method " << meta.name << '(';

  for (size_t i = 0; i < meta.params.size(); ++i) {
    if (i != 0) {
      stream() << ", ";
    }

    writeParam(meta.params[i]);
  }

  stream() << ") -> ";

  writeTypeRef(meta.returnType);

  if (!meta.initializedFields.empty()) {
    stream() << " init[";

    for (size_t i = 0; i < meta.initializedFields.size(); ++i) {
      if (i != 0) {
        stream() << ", ";
      }

      stream() << meta.initializedFields[i];
    }

    stream() << ']';
  }

  stream() << ";\n";
}

// ============================================================
// Parameter
// ============================================================

void MetaWriter::writeParam(const ParamMeta &meta) {
  stream() << meta.name << ": ";

  writeTypeRef(meta.type);

  if (meta.defaultValue.has_value()) {
    stream() << " = ";
    writeDefaultValue(*meta.defaultValue);
  }
}

// ============================================================
// Enum Variant
// ============================================================

void MetaWriter::writeVariant(const EnumVariantMeta &meta) {
  writeIndent();

  stream() << meta.name;

  if (meta.payload.has_value()) {
    stream() << '[';
    writeTypeRef(*meta.payload);
    stream() << ']';
  }

  stream() << ";\n";
}

// ============================================================
// TypeRef
// ============================================================

void MetaWriter::writeTypeRef(const TypeRef &ref) {
  if (out == nullptr) {
    Error::internal("MetaWriter output stream is not initialized");
  }

  switch (ref.kind) {
  case TypeRefKind::BuiltIn: {
    if (!ref.builtIn.has_value()) {
      Error::internal("builtin TypeRef has no builtin type");
    }

    stream() << builtInName(*ref.builtIn);
    return;
  }

  case TypeRefKind::Declared: {
    if (ref.name.empty()) {
      Error::internal("declared TypeRef has empty name");
    }

    writeQualifiedName(ref.path, ref.name);
    return;
  }

  case TypeRefKind::Generic: {
    if (ref.name.empty()) {
      Error::internal("generic TypeRef has empty name");
    }

    writeQualifiedName(ref.path, ref.name);
    stream() << '<';

    for (size_t i = 0; i < ref.args.size(); ++i) {
      if (i != 0) {
        stream() << ", ";
      }

      writeTypeRef(ref.args[i]);
    }

    stream() << '>';
    return;
  }

  case TypeRefKind::Array: {
    if (ref.args.size() != 1) {
      Error::internal("array TypeRef must contain exactly one element type");
    }

    if (!ref.arraySize.has_value()) {
      Error::internal("array TypeRef has no array size");
    }

    writeTypeRef(ref.args.front());

    llvm::SmallString<32> buffer;
    ref.arraySize->toString(buffer, 10, false);

    stream() << '[' << buffer.c_str() << ']';
    return;
  }
  }

  Error::internal("unknown TypeRefKind");
}
// ============================================================
// Default Value
// ============================================================

void MetaWriter::writeDefaultValue(const DefaultValueMeta &value) {
  if (out == nullptr) {
    Error::internal("MetaWriter output stream is not initialized");
  }

  writeTypeRef(value.resolvedType);
  stream() << ": ";

  switch (value.kind) {
  case DefaultValueKind::Literal: {
    if (!value.literal.has_value()) {
      Error::internal("literal default value has no literal");
    }

    const ResolvedLit &literal = *value.literal;

    // Integer literal signedness is now restored from
    // DefaultValueMeta::resolvedType instead of ResolvedLit::type. Imported
    // metadata intentionally has no TypeSymbol attached to ResolvedLit yet.
    if (literal.isInt()) {
      if (value.resolvedType.kind != TypeRefKind::BuiltIn ||
          !value.resolvedType.builtIn.has_value()) {
        Error::internal("integer default value has non-builtin resolved type");
      }

      const BuiltInType builtin = *value.resolvedType.builtIn;

      switch (builtin) {
      case BuiltInType::I8:
      case BuiltInType::I16:
      case BuiltInType::I32:
      case BuiltInType::I64:
      case BuiltInType::I128:
      case BuiltInType::U8:
      case BuiltInType::U16:
      case BuiltInType::U32:
      case BuiltInType::U64:
      case BuiltInType::U128:
        break;

      default:
        Error::internal("integer default value has non-integer resolved type");
      }

      llvm::SmallString<64> buffer;
      literal.asInt().value.toString(buffer, 10, isSignedInteger(builtin));
      stream() << buffer.c_str();
      return;
    }

    writeLiteral(literal);
    return;
  }

  case DefaultValueKind::StructInit:
    if (!value.type.has_value()) {
      Error::internal("struct init default value has no type");
    }

    writeTypeRef(*value.type);

    stream() << '(';

    for (size_t i = 0; i < value.args.size(); ++i) {
      if (i != 0) {
        stream() << ", ";
      }

      writeDefaultValue(value.args[i]);
    }

    stream() << ')';
    return;
  }

  Error::internal("unknown DefaultValueKind");
}
// ============================================================
// Literal
// ============================================================
void MetaWriter::writeLiteral(const ResolvedLit &lit) {
  if (lit.isBool()) {
    stream() << (lit.asBool() ? "true" : "false");
    return;
  }

  if (lit.isInt()) {
    llvm::SmallString<64> buffer;

    if (rewrite) {
      lit.asInt().value.toString(buffer, 10, true);
      stream() << buffer.c_str();
      return;
    }

    if (lit.type == nullptr) {
      Error::internal("integer ResolvedLit has no resolved type");
    }

    const auto *primitive = dynamic_cast<const PrimtiveType *>(lit.type);
    if (primitive == nullptr) {
      Error::internal("integer ResolvedLit type is not a primitive type");
    }

    switch (primitive->builtinType) {
    case BuiltInType::I8:
    case BuiltInType::I16:
    case BuiltInType::I32:
    case BuiltInType::I64:
    case BuiltInType::I128:
    case BuiltInType::U8:
    case BuiltInType::U16:
    case BuiltInType::U32:
    case BuiltInType::U64:
    case BuiltInType::U128:
      break;

    default:
      Error::internal("integer ResolvedLit has non-integer builtin type");
    }

    lit.asInt().value.toString(buffer, 10,
                               isSignedInteger(primitive->builtinType));

    stream() << buffer.c_str();
    return;
  }

  if (lit.isFloat()) {
    llvm::SmallString<64> buffer;
    lit.asFloat().value.toString(buffer);

    stream() << buffer.c_str();
    return;
  }

  if (lit.isChar()) {
    stream() << '\'';
    writeEscapedChar(stream(), lit.asChar().codePoint);
    stream() << '\'';
    return;
  }

  if (lit.isString()) {
    stream() << '"';
    writeEscapedString(stream(), lit.asString().codePoints);
    stream() << '"';
    return;
  }

  Error::internal("unknown ResolvedLit kind");
}

// ============================================================
// Path
// ============================================================

void MetaWriter::writePath(const SourcePath &path) {
  for (size_t i = 0; i < path.segments.size(); ++i) {
    if (i != 0) {
      stream() << "::";
    }

    stream() << path.segments[i];
  }
}

void MetaWriter::writeQualifiedName(const SourcePath &path,
                                    std::string_view name) {
  if (!path.segments.empty()) {
    writePath(path);
    stream() << "::";
  }

  stream() << name;
}

// ============================================================
// Formatting
// ============================================================

void MetaWriter::writeIndent() {
  for (unsigned i = 0; i < indentLevel; ++i) {
    stream() << "  ";
  }
}

// ============================================================
// TypeKind
// ============================================================

std::string_view MetaWriter::typeKindName(TypeKind kind) {
  switch (kind) {
  case TypeKind::Enum:
    return "enum";

  case TypeKind::Class:
    return "class";

  case TypeKind::Struct:
    return "struct";
  }

  Error::internal("unknown metadata TypeKind");
}

// ============================================================
// Access Modifier
// ============================================================

std::string_view MetaWriter::modifierName(AModifier modifier) {
  switch (modifier) {
  case AModifier::PUBLIC:
    return "public";

  case AModifier::PROTECTED:
    return "protected";

  case AModifier::PRIVATE:
    return "private";
  }

  Error::internal("unknown access modifier");
}

// ============================================================
// BuiltIn
// ============================================================

std::string_view MetaWriter::builtInName(BuiltInType type) {
  switch (type) {
  case BuiltInType::I8:
    return "i8";
  case BuiltInType::I16:
    return "i16";
  case BuiltInType::I32:
    return "i32";
  case BuiltInType::I64:
    return "i64";
  case BuiltInType::I128:
    return "i128";

  case BuiltInType::U8:
    return "u8";
  case BuiltInType::U16:
    return "u16";
  case BuiltInType::U32:
    return "u32";
  case BuiltInType::U64:
    return "u64";
  case BuiltInType::U128:
    return "u128";

  case BuiltInType::F16:
    return "f16";
  case BuiltInType::F32:
    return "f32";
  case BuiltInType::F64:
    return "f64";
  case BuiltInType::F128:
    return "f128";

  case BuiltInType::C8:
    return "c8";
  case BuiltInType::C16:
    return "c16";
  case BuiltInType::C32:
    return "c32";

  case BuiltInType::S8:
    return "s8";
  case BuiltInType::S16:
    return "s16";
  case BuiltInType::S32:
    return "s32";

  case BuiltInType::B:
    return "bool";

  case BuiltInType::VOID:
    return "void";

  case BuiltInType::FI:
    return "fi";
  }

  Error::internal("unknown BuiltInType");
}

bool MetaWriter::isSignedInteger(BuiltInType type) {
  switch (type) {
  case BuiltInType::I8:
  case BuiltInType::I16:
  case BuiltInType::I32:
  case BuiltInType::I64:
  case BuiltInType::I128:
    return true;

  case BuiltInType::U8:
  case BuiltInType::U16:
  case BuiltInType::U32:
  case BuiltInType::U64:
  case BuiltInType::U128:
    return false;

  default:
    Error::internal("non integer builtin used as integer literal type");
  }
}

// ============================================================
// Literal escaping
// ============================================================

void MetaWriter::writeEscapedChar(std::ostream &out, uint32_t codePoint) {
  switch (codePoint) {
  case '\n':
    out << "\\n";
    return;

  case '\r':
    out << "\\r";
    return;

  case '\t':
    out << "\\t";
    return;

  case '\\':
    out << "\\\\";
    return;

  case '\'':
    out << "\\'";
    return;

  case '"':
    out << "\\\"";
    return;

  default:
    break;
  }

  if (codePoint >= 0x20 && codePoint <= 0x7E) {
    out << static_cast<char>(codePoint);
    return;
  }

  // Non-ASCII 문자는 \u{...} 형식으로 저장.
  // Reader에서도 동일한 escape 규칙을 처리하면 된다.
  const auto flags = out.flags();

  out << "\\u{" << std::hex << std::uppercase << codePoint << '}';

  out.flags(flags);
}

void MetaWriter::writeEscapedString(std::ostream &out,
                                    const std::vector<uint32_t> &codePoints) {
  for (uint32_t codePoint : codePoints) {
    writeEscapedChar(out, codePoint);
  }
}