#pragma once

#include "string"

using std::string;

enum class HIRTypeKind {
  Void,
  Error,

  Builtin,
  Struct,
  Entity,
  Enum,

  Handle,
  Observer,
};

enum class StorageKind {
  World,
  Arena,
  Resource,
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
  HIRErrorType() : HIRType(HIRTypeKind::Error, "<error>") {}
};

enum class BuiltinTypeKind {
  Bool,
  Char,
  String,
  Int,
  Float,
};

struct HIRBuiltinType : HIRType {
  BuiltinTypeKind builtinKind;

  HIRBuiltinType(std::string n, BuiltinTypeKind b)
      : HIRType(HIRTypeKind::Builtin, std::move(n)), builtinKind(b) {}
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