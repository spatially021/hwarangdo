#pragma once

#include "../AST/ASTNode.h"
#include "../AST/Decl.h"
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

using std::string;
using std::vector;

class ValueSymbol;
class EnumVariantSymbol;
struct Scope;

class Symbol {
public:
  string name;
  enum class SymbolType {
    TYPE,
    VALUE,
  } type;
  virtual string getName();
};

class TypeSymbol : public Symbol {
public:

  TypeSymbol(){
    type=Symbol::SymbolType::TYPE;
  }

  enum class Kind {
    CLASS,
    ENUM,
    STRUCT,
    TRAIT,
    BUILTIN,
  } kind;

  Decl *decl;
  TypeSymbol *base;
  optional<string> baseName;

  //class/struct
  Scope * memberScope;

  //enum
  vector<unique_ptr<EnumVariantSymbol>> variants;
  unordered_map<string, EnumVariantSymbol*> variantMap;

  string getName() { return name; }
};

class ValueSymbol : public Symbol {
public:

ValueSymbol(){
  type=Symbol::SymbolType::VALUE;
}

  enum class Kind {
    VAR,
    METHOD,
    TRAITSIG,
    PARAM,
  } kind;

  ASTNode * node;

  string getName() { return name; }
};

class EnumVariantSymbol : public Symbol {
public:
  EnumDecl::Variant *variant;
  int ordinal;
};

class ImplSymbol:public Symbol{
  public:
  string targetName;
  TypeSymbol * target;
  Decl *decl;
  Scope *member;
};


struct Scope {
  Scope *parent = nullptr;
  vector<std::unique_ptr<Scope>> children;

  unordered_map<string, unique_ptr<TypeSymbol>> type;
  unordered_map<string, unique_ptr<ValueSymbol>> value;
};