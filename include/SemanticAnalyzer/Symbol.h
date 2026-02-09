#pragma once

#include "AST/ASTNode.h"
#include "AST/Decl.h"
#include "Token.h"
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

using std::string;
using std::vector;

class ValueSymbol;
class EnumVariantSymbol;

class Symbol {
public:
  string name;
  enum class SymbolType {
    TYPE,
    VALUE,
    IMPL,
    METHOD,
  } type;
  virtual ~Symbol() = default;

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
    BUILTIN,
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

protected:
  void _anchor() override {};
};

class MethodSymbol : public Symbol {
public:
  MethodSymbol() { type = Symbol::SymbolType::METHOD; }

  TypeSymbol *onwer = nullptr;
  TypeSymbol *returnType = nullptr;
  ASTNode *decl = nullptr;
  vector<TypeSymbol> paramTypes;
  Expr::State state = Expr::State::RESOLVED;
  Scope *scope = nullptr;
  Scope *selfScope = nullptr;
  std::vector<ReturnStmt*> returns;

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

  ASTNode *node = nullptr;
  TypeSymbol *typeSymbol = nullptr;

protected:
  void _anchor() override {};
};

class EnumVariantSymbol : public Symbol {
public:
  EnumDecl::Variant *variant = nullptr;
  int ordinal;
  TypeSymbol *payloadType = nullptr;

  EnumVariantSymbol() { type = Symbol::SymbolType::VALUE; }

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
