#pragma once

#include "Symbol.h"
#include "hrd/AST/Decl.h"
#include "hrd/BuiltInType.h"
#include "hrd/Inputs.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SourceSpan.h"
#include "hrd/enums/StorageKind.h"
#include "hrd/util/Error.h"
#include <cstdint>
#include <llvm/ADT/APInt.h>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

class Scope;
struct Module;

class TypeSymbol : public Symbol {
public:
  TypeSymbol();
  ~TypeSymbol();
  enum class TypeKind {
    CLASS,
    ENUM,
    STRUCT,
    TRAIT,
    PRIMITIVE,
    VOID,
    FUNC,
    HANDLE,
    RESULT,
    OPTION,
    ERROR,
    UNKNOWN,
    BUILTIN,
    DEFAULT_VALUE,
    ARRAY,
    GENERIC,
    ROOT,
  } kind;

  Decl *decl = nullptr;
  TypeSymbol *base = nullptr;
  optional<string> baseName = nullopt;
  unordered_set<TypeSymbol *> traits;
  unordered_map<TypeSymbol *, SourceSpan> traitSpan;
  Module *module = nullptr;
  SourcePath path;

  // class/struct
  Scope *memberScope = nullptr;
  vector<ValueSymbol *> fields;
  uint32_t fieldCount = 0;

  // struct
  unordered_map<string, vector<MethodSymbol *>> impledMethod;

  // enum
  vector<unique_ptr<EnumVariantSymbol>> variants;
  unordered_map<string, EnumVariantSymbol *> variantMap;

  bool isInhereted = false;
  bool isReserved = false;

  // trait
  unordered_map<string, vector<TraitSig *>> traitSigs;

  pair<bool, SourceSpan> addMethod(MethodSymbol *symbol);

protected:
  void _anchor() override {};
};

class RootSymbol : public TypeSymbol {
public:
  RootSymbol() { kind = TypeKind::ROOT; }

protected:
  void _anchor() override {};
};

class ErrorType : public TypeSymbol {
public:
protected:
  void _anchor() override {};
};

class MainSymbol : public TypeSymbol {
public:
  Decl *decl = nullptr;
  MethodSymbol *update = nullptr;
  MethodSymbol *init = nullptr;
  MainSymbol();
  ~MainSymbol();

protected:
  void _anchor() override {};
};

class ImplSymbol : public TypeSymbol {
public:
  string targetName;
  TypeSymbol *target = nullptr;
  ImplSymbol() { type = Symbol::SymbolType::IMPL; }

protected:
  void _anchor() override {}
};

class PrimtiveType : public TypeSymbol {
public:
  BuiltinCategory builtinCategory;
  BuiltInType builtinType;

  PrimtiveType(enum BuiltinCategory pk, BuiltInType ty) : builtinType(ty) {
    kind = TypeSymbol::TypeKind::PRIMITIVE;
    builtinCategory = pk;
    isReserved = true;
  }

  static bool classof(const TypeSymbol *type) {
    return type->kind == TypeSymbol::TypeKind::PRIMITIVE;
  }

protected:
  void _anchor() override {}
};

class IntType : public PrimtiveType {
public:
  unsigned int bitWidth = 32;
  bool isSigned = true;

  IntType(BuiltInType t = {}) : PrimtiveType(BuiltinCategory::Int, t) {
    switch (t) {
    case BuiltInType::I8:
      bitWidth = 8;
      name = "i8";
      break;
    case BuiltInType::I16:
      bitWidth = 16;
      name = "i16";
      break;
    case BuiltInType::I32:
      bitWidth = 32;
      name = "i32";
      break;
    case BuiltInType::I64:
      bitWidth = 64;
      name = "i64";
      break;
    case BuiltInType::I128:
      bitWidth = 128;
      name = "i128";
      break;
    case BuiltInType::U8:
      isSigned = false;
      bitWidth = 8;
      name = "u8";
      break;
    case BuiltInType::U16:
      isSigned = false;
      bitWidth = 16;
      name = "u16";
      break;
    case BuiltInType::U32:
      isSigned = false;
      bitWidth = 32;
      name = "u32";
      break;
    case BuiltInType::U64:
      isSigned = false;
      bitWidth = 64;
      name = "u64";
      break;
    case BuiltInType::U128:
      isSigned = false;
      bitWidth = 128;
      name = "u128";
      break;
    default:
      Error::internal("unmatched size in ineteger type");
    }
  }

  static bool classof(const TypeSymbol *type) {
    if (!PrimtiveType::classof(type)) {
      return false;
    }

    return static_cast<const PrimtiveType *>(type)->builtinCategory ==
           BuiltinCategory::Int;
  }
};

class FloatType : public PrimtiveType {
public:
  unsigned int bitWidth = 32;
  unsigned int precious = 24;

  FloatType(BuiltInType t = {}) : PrimtiveType(BuiltinCategory::Float, t) {
    switch (t) {

    case BuiltInType::F16:
      bitWidth = 16;
      precious = 11;
      name = "f16";
      break;
    case BuiltInType::F32:
      bitWidth = 32;
      precious = 24;
      name = "f32";
      break;
    case BuiltInType::F64:
      bitWidth = 64;
      precious = 53;
      name = "f64";
      break;
    case BuiltInType::F128:
      bitWidth = 128;
      precious = 113;
      name = "f128";
      break;
    default:
      Error::internal("unmatched size in float type");
      break;
    }
  }
  static bool classof(const TypeSymbol *type) {
    if (!PrimtiveType::classof(type)) {
      return false;
    }

    return static_cast<const PrimtiveType *>(type)->builtinCategory ==
           BuiltinCategory::Float;
  }
};

class BoolType : public PrimtiveType {
public:
  BoolType() : PrimtiveType(BuiltinCategory::Bool, BuiltInType::B) {
    name = "bool";
  }
  static bool classof(const TypeSymbol *type) {
    if (!PrimtiveType::classof(type)) {
      return false;
    }

    return static_cast<const PrimtiveType *>(type)->builtinCategory ==
           BuiltinCategory::Bool;
  }
};

class CharType : public PrimtiveType {
public:
  unsigned int bitWidth = 8;
  CharType(BuiltInType t = {}) : PrimtiveType(BuiltinCategory::Char, t) {
    switch (t) {

    case BuiltInType::C8:
      bitWidth = 8;
      name = "c8";
      break;
    case BuiltInType::C16:
      bitWidth = 16;
      name = "c16";
      break;
    case BuiltInType::C32:
      bitWidth = 32;
      name = "c32";
      break;
    default:
      Error::internal("unmatched size in char type");
    }
  }
  static bool classof(const TypeSymbol *type) {
    if (!PrimtiveType::classof(type)) {
      return false;
    }

    return static_cast<const PrimtiveType *>(type)->builtinCategory ==
           BuiltinCategory::Char;
  }
};

class StringType : public PrimtiveType {
public:
  unsigned int bitWidth = 8;
  StringType(BuiltInType t = {}) : PrimtiveType(BuiltinCategory::String, t) {
    switch (t) {
    case BuiltInType::S8:
      bitWidth = 8;
      name = "s8";
      break;
    case BuiltInType::S16:
      bitWidth = 16;
      name = "s16";
      break;
    case BuiltInType::S32:
      bitWidth = 32;
      name = "s32";
      break;
    default:
      Error::internal("unmatched size in string type");
    }
  }
  static bool classof(const TypeSymbol *type) {
    if (!PrimtiveType::classof(type)) {
      return false;
    }

    return static_cast<const PrimtiveType *>(type)->builtinCategory ==
           BuiltinCategory::String;
  }
};

class HandleSymbol : public TypeSymbol {
public:
  StorageKind storage = StorageKind::World;

  HandleSymbol() {
    isReserved = true;
    kind = TypeSymbol::TypeKind::HANDLE;
  }

  static bool classof(const TypeSymbol *type) {
    return type->kind == TypeSymbol::TypeKind::HANDLE;
  }
};

class ResultSymbol : public TypeSymbol {
public:
  ResultSymbol() {
    isReserved = true;
    kind = TypeSymbol::TypeKind::RESULT;
  }

  static bool classof(const TypeSymbol *type) {
    return type->kind == TypeSymbol::TypeKind::RESULT;
  }
};

class OptionSymbol : public TypeSymbol {
public:
  OptionSymbol() {
    isReserved = true;
    kind = TypeSymbol::TypeKind::OPTION;
  }

  static bool classof(const TypeSymbol *type) {
    return type->kind == TypeSymbol::TypeKind::OPTION;
  }
};

class GenericSymbol : public TypeSymbol {
public:
  TypeSymbol *origin = nullptr;
  std::vector<TypeSymbol *> args;

  GenericSymbol(TypeSymbol *o, std::vector<TypeSymbol *> a);
  ~GenericSymbol();

  static bool classof(const TypeSymbol *type) {
    return type->kind == TypeSymbol::TypeKind::GENERIC;
  }
};

class ArrayTypeSymbol : public TypeSymbol {
public:
  TypeSymbol *baseType = nullptr;
  llvm::APInt sizeValue;

  ArrayTypeSymbol(TypeSymbol *b, llvm::APInt s)
      : TypeSymbol(), baseType(b), sizeValue(std::move(s)) {
    kind = TypeSymbol::TypeKind::ARRAY;
  }

  static bool classof(const TypeSymbol *type) {
    return type->kind == TypeSymbol::TypeKind::ARRAY;
  }
};

template <typename T> bool isa(const TypeSymbol *type) {
  return type != nullptr && T::classof(type);
}

template <TypeSymbol::TypeKind K> bool isKind(const TypeSymbol *type) {
  return type != nullptr && type->kind == K;
}

template <BuiltInType B> bool isBuiltin(const TypeSymbol *type) {
  if (!type || type->kind != TypeSymbol::TypeKind::PRIMITIVE) {
    return false;
  }

  const auto *primitive = static_cast<const PrimtiveType *>(type);
  return primitive->builtinType == B;
}

template <typename T> T *cast(TypeSymbol *type) {
  assert(isa<T>(type));
  return static_cast<T *>(type);
}

template <typename T> const T *cast(const TypeSymbol *type) {
  assert(isa<T>(type));
  return static_cast<const T *>(type);
}

template <typename T> T *dyn_cast(TypeSymbol *type) {
  return isa<T>(type) ? static_cast<T *>(type) : nullptr;
}

template <typename T> const T *dyn_cast(const TypeSymbol *type) {
  return isa<T>(type) ? static_cast<const T *>(type) : nullptr;
}
