#pragma once

#include "AST/Decl.h"
#include "BuiltInType.h"
#include "IR/HIR/HIRType.h"
#include "SemanticAnalyzer/symbol/ValueSymbol.h"
#include "Symbol.h"
#include "enums/StorageKind.h"
#include "util/Error.h"
#include <llvm/ADT/APInt.h>
#include <optional>
#include <string>
#include <vector>

class Scope;

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
  } kind;

  Decl *decl = nullptr;
  TypeSymbol *base = nullptr;
  optional<string> baseName = nullopt;
  vector<TypeSymbol *> traits;

  // class/struct
  Scope *memberScope = nullptr;
  // struct
  unordered_map<string, vector<MethodSymbol *>> impledMethod;

  // enum
  vector<unique_ptr<EnumVariantSymbol>> variants;
  unordered_map<string, EnumVariantSymbol *> variantMap;

  bool isInhereted = false;
  bool isReserved = false;

  // trait
  unordered_map<string, vector<TraitSig *>> traitSigs;

  bool addMethod(MethodSymbol *symbol);

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
  unique_ptr<Scope> rootScope;
  MethodSymbol *main = nullptr;
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
  PrimtiveType(enum BuiltinCategory pk) {
    kind = TypeSymbol::TypeKind::PRIMITIVE;
    builtinCategory = pk;
    isReserved = true;
  }

protected:
  void _anchor() override {}
};

class IntType : public PrimtiveType {
public:
  int bitWidth = 32;
  bool isSigned = true;
  IntType(BuiltInType t = {}) : PrimtiveType(BuiltinCategory::Int) {
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
};

class FloatType : public PrimtiveType {
public:
  int bitWidth = 32;
  int precious = 24;

  FloatType(BuiltInType t = {}) : PrimtiveType(BuiltinCategory::Float) {
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
};

class BoolType : public PrimtiveType {
public:
  BoolType() : PrimtiveType(BuiltinCategory::Bool) { name = "bool"; }
};

class CharType : public PrimtiveType {
public:
  int bitWidth = 8;
  CharType(BuiltInType t = {}) : PrimtiveType(BuiltinCategory::Char) {
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
};

class StringType : public PrimtiveType {
public:
  int bitWidth = 8;
  StringType(BuiltInType t = {}) : PrimtiveType(BuiltinCategory::String) {
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
};

class HandleSymbol : public TypeSymbol {
public:
  StorageKind storage = StorageKind::World;
  HandleSymbol() {
    isReserved = true;
    kind = TypeSymbol::TypeKind::HANDLE;
  }
};

class ResultSymbol : public TypeSymbol {
public:
  ResultSymbol() {
    isReserved = true;
    kind = TypeSymbol::TypeKind::RESULT;
  }
};

class OptionSymbol : public TypeSymbol {
public:
  OptionSymbol() {
    isReserved = true;
    kind = TypeSymbol::TypeKind::OPTION;
  }
};

class GenericSymbol : public TypeSymbol {
public:
  TypeSymbol *origin = nullptr;
  std::vector<TypeSymbol *> args;
  GenericSymbol(TypeSymbol *o, std::vector<TypeSymbol *> a);
  ~GenericSymbol();
};

class ArrayTypeSymbol : public TypeSymbol {
public:
  TypeSymbol *baseType = nullptr;
  llvm::APInt sizeValue;
  ArrayTypeSymbol(TypeSymbol *b, llvm::APInt s)
      : TypeSymbol(), baseType(b), sizeValue(s) {
    kind = TypeSymbol::TypeKind::ARRAY;
  }
};