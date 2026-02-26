#pragma once

#include "AST/ASTNode.h"
#include "AST/Decl.h"
#include "AST/Expr.h"
#include "BuiltInType.h"
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

using std::string;
using std::unordered_map;
using std::vector;

class ValueSymbol;
class EnumVariantSymbol;
class Decl;
class Scope;

class Symbol {
public:
  string name;
  enum class SymbolType {
    TYPE,
    VALUE,
    IMPL,
    METHOD,
    ENUM_VARIANT,
  } type;
  virtual ~Symbol() = default;

  enum class OwnType {
    Owned,
    Borrowed,
    Moved,
    Reference,
  } own = Symbol::OwnType::Owned;

protected:
  virtual void _anchor() = 0;
};

class TypeSymbol : public Symbol {
public:
  TypeSymbol() { type = Symbol::SymbolType::TYPE; }

  enum class Kind {
    CLASS,
    ENUM,
    STRUCT,
    TRAIT,
    PRIMITIVE,
    VOID,
    FUNC,
    UNKNOWN,
  } kind;

  Decl *decl = nullptr;
  TypeSymbol *base = nullptr;
  optional<string> baseName;
  vector<TypeSymbol *> traits;
  // class/struct
  Scope *memberScope = nullptr;

  // enum
  vector<unique_ptr<EnumVariantSymbol>> variants;
  unordered_map<string, EnumVariantSymbol *> variantMap;
  unordered_map<string, Token> methodsName;

  bool isInhereted = false;

protected:
  void _anchor() override {};
};

class MethodSymbol : public Symbol {
public:
  MethodSymbol() { type = Symbol::SymbolType::METHOD; }

  TypeSymbol *onwer = nullptr;
  TypeSymbol *returnType = nullptr;
  ASTNode *decl = nullptr;
  vector<TypeSymbol *> paramTypes;
  Expr::State state = Expr::State::RESOLVED;
  Scope *scope = nullptr;
  Scope *selfScope = nullptr;
  std::vector<ReturnStmt *> returns;

protected:
  void _anchor() override {};
};

class ValueSymbol : public Symbol {
public:
  ValueSymbol() { type = Symbol::SymbolType::VALUE; }

  enum class Kind {
    VAR,
    TRAITSIG,
    PARAM,
  } kind;

  enum class BindingType {
    Value,
    Reference,
    Borrow,
  };

  ASTNode *node = nullptr;
  TypeSymbol *typeSymbol = nullptr;

protected:
  void _anchor() override {};
};

class EnumVariantSymbol : public ValueSymbol {
public:
  int ordinal;
  TypeSymbol *payloadType = nullptr;
  optional<ValueSymbol> payloadValue = nullopt;
  EnumVariantSymbol() { type = Symbol::SymbolType::ENUM_VARIANT; }

protected:
  void _anchor() override {};
};

class ImplSymbol : public TypeSymbol {
public:
  string targetName;
  TypeSymbol *target = nullptr;
  Decl *decl = nullptr;
  ImplSymbol() { type = Symbol::SymbolType::IMPL; }

protected:
  void _anchor() override {}
};

class PrimtiveType : public TypeSymbol {
public:
  enum class PrimtiveKind {
    INT,
    FLOAT,
    CHAR,
    STRING,
    BOOL,
    FIXED,

  } primtiveKind;
  PrimtiveType(PrimtiveKind pk) {
    kind = TypeSymbol::Kind::PRIMITIVE;
    primtiveKind = pk;
  }

protected:
  void _anchor() override {}
};

class IntType : public PrimtiveType {
public:
  int bitWidth = 32;
  bool singed = true;
  IntType(BuiltInType t = {}) : PrimtiveType(PrimtiveKind::INT) {
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
      singed = false;

      bitWidth = 8;
      name = "u8";

      break;
    case BuiltInType::U16:
      singed = false;
      bitWidth = 16;
      name = "u16";

      break;
    case BuiltInType::U32:
      singed = false;
      bitWidth = 32;
      name = "u32";

      break;
    case BuiltInType::U64:
      singed = false;
      bitWidth = 64;
      name = "u64";

      break;
    case BuiltInType::U128:
      singed = false;
      bitWidth = 128;
      name = "u128";

      break;
    default:
      throw("unmatched size");
    }
  }
};

class FloatType : public PrimtiveType {
public:
  int bitWidth = 32;
  int precious = 24;

  FloatType(BuiltInType t = {}) : PrimtiveType(PrimtiveKind::FLOAT) {
    switch (t) {

    case BuiltInType::F16:
      bitWidth = 16;
      precious = 24;
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
      throw("unmatched size");
      break;
    }
  }
};

class BoolType : public PrimtiveType {
public:
  BoolType() : PrimtiveType(PrimtiveKind::BOOL) {}
};

class CharType : public PrimtiveType {
public:
  int bitWidth = 8;
  CharType(BuiltInType t = {}) : PrimtiveType(PrimtiveKind::CHAR) {
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
      throw("unmatched size");
    }
  }
};

class StringType : public PrimtiveType {
public:
  int bitWidth = 8;
  StringType(BuiltInType t = {}) : PrimtiveType(PrimtiveKind::STRING) {
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
      throw("unmatched size");
    }
  }
};