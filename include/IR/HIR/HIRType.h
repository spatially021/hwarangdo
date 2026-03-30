#pragma once

#include "SemanticAnalyzer/ResolvedLit.h"
#include "enums/BuiltInCategory.h"
#include "enums/StorageKind.h"
#include "string"

using std::string;

enum class HIRTypeKind {
  Void,

  Result,
  Option,
  Error,

  Builtin,
  Struct,
  Entity,
  Enum,

  Handle,
  Observer,

  NUL,
};

class TypeSymbol;

struct HIRType {
  HIRTypeKind kind;
  string name;

  explicit HIRType(HIRTypeKind k, string n) : kind(k), name(std::move(n)) {}
  virtual ~HIRType() = default;
};

struct HIRVoidType : HIRType {
  HIRVoidType() : HIRType(HIRTypeKind::Void, "void") {}
};

struct HIRErrorType : HIRType {
  string errorInfo = "";
  HIRErrorType() : HIRType(HIRTypeKind::Error, "<error>") {}
};

struct HIRBuiltinType : HIRType {
  BuiltinCategory builtinKind;
  TypeSymbol *symbol;

  HIRBuiltinType(std::string n, BuiltinCategory b, TypeSymbol *t)
      : HIRType(HIRTypeKind::Builtin, std::move(n)), builtinKind(b), symbol(t) {
  }
};

struct HIRStructType : HIRType {
  TypeSymbol *symbol = nullptr;

  explicit HIRStructType(std::string n, TypeSymbol *s = nullptr)
      : HIRType(HIRTypeKind::Struct, std::move(n)), symbol(s) {}
};

struct HIREntityType : HIRType {
  TypeSymbol *symbol = nullptr;

  explicit HIREntityType(std::string n, TypeSymbol *s = nullptr)
      : HIRType(HIRTypeKind::Entity, std::move(n)), symbol(s) {}
};

struct HIREnumType : HIRType {
  TypeSymbol *symbol = nullptr;

  explicit HIREnumType(std::string n, TypeSymbol *s = nullptr)
      : HIRType(HIRTypeKind::Enum, std::move(n)), symbol(s) {}
};

struct HIRHandleType : HIRType {
  HIREntityType *entityType = nullptr;
  StorageKind storage = StorageKind::World;

  HIRHandleType(HIREntityType *e, StorageKind s)
      : HIRType(HIRTypeKind::Handle, "Handle<" + e->name + ">"), entityType(e),
        storage(s) {}
};

struct HIROserverType : HIRType {
  HIREntityType *entityType = nullptr;
  StorageKind storage = StorageKind::World;

  HIROserverType(HIREntityType *e, StorageKind s)
      : HIRType(HIRTypeKind::Observer, "Observer<" + e->name + ">"),
        entityType(e), storage(s) {}
};

struct HIRResultType : HIRType {
  HIRType *successType = nullptr;
  HIRErrorType *error = nullptr;

  HIRResultType(HIRType *s, HIRErrorType *e)
      : HIRType(HIRTypeKind::Result,
                "Result<" + s->name + ", " + e->name + ">"),
        successType(s), error(e) {}
};

struct HIROptionType : HIRType {
  HIRType *type = nullptr;

  HIROptionType(HIRType *t)
      : HIRType(HIRTypeKind::Option, "Option<" + t->name + ">"), type(t) {}
};
