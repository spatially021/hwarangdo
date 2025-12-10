#pragma once
#include "AST/Expr.h"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// ────────── 기본 Symbol 클래스 ──────────
struct Symbol {
  std::string name;
  int scopeLevel;
  std::string type;
  virtual ~Symbol() = default;
};

// ────────── 변수 심볼 ──────────
struct VarSymbol : public Symbol {
  
  bool isConst = false;
  std::string initialValue;

  VarSymbol(const std::string &n, const std::string &t, bool c, int lvl)
      :  isConst(c) {
        type=t;
    name = n;
    scopeLevel = lvl;
  }
};

struct ArraySymbol : public VarSymbol {
  Expr::Ptr index;
  ArraySymbol(const std::string &n, const std::string &t, bool c, int lvl,
              Expr::Ptr i)
      : VarSymbol(n, t, c, lvl) {
    index = i;
  }
};

struct ObjectSymbol :public VarSymbol{
  std::string className;
  ObjectSymbol(const std::string &n, const std::string &t, bool c, int lvl)
      : VarSymbol(n, t, c, lvl) {
    className=t;
  }
};

// ────────── 함수 심볼 ──────────
struct FuncSymbol : public Symbol {
  std::string returnType;
  std::vector<std::string> paramTypes;

  FuncSymbol(const std::string &n, const std::string &ret,
             const std::vector<std::string> &params, int lvl)
      : returnType(ret), paramTypes(params) {
    name = n;
    scopeLevel = lvl;
    type="function";
  }
};

// 수정된/추가된 부분만 발췌한 SymbolTable 관련 코드

struct ClassSymbol : public Symbol {
  // name은 base Symbol에 있음
  // 클래스 멤버(필드/메서드)를 보관
  std::unordered_map<std::string, std::shared_ptr<Symbol>> members;

  ClassSymbol(const std::string &n) { name = n; }

  bool hasMember(const std::string &m) const {
    return members.find(m) != members.end();
  }

  // 멤버를 추가 (이미 있으면 false)
  bool addMember(const std::shared_ptr<Symbol> &sym) {
    if (!sym)
      return false;
    if (members.count(sym->name))
      return false;
    members[sym->name] = sym;
    return true;
  }

  std::shared_ptr<Symbol> getMember(const std::string &m) const {
    auto it = members.find(m);
    if (it == members.end())
      return nullptr;
    return it->second;
  }
};

struct SymbolInfo {
  std::string name;
  std::string type;

  bool isConst;
  std::string initalValue;

  Expr::Ptr index;

  std::vector<std::string> paramTypes;
};

enum SymbolKind {
  VAR,
  FUNC,
  CLASS,
  ARRAY,
  OBJECT,
};

class SymbolTable {
  struct Scope {
    std::unordered_map<std::string, std::shared_ptr<VarSymbol>> vars;
    std::unordered_map<std::string, std::shared_ptr<FuncSymbol>> funcs;
    std::unordered_map<std::string, std::shared_ptr<ClassSymbol>> classes;
  };

  std::vector<Scope> scopes;

public:
  SymbolTable() { enterScope(); }

  void enterScope() { scopes.push_back({}); }
  void exitScope() {
    Scope exited = scopes.back();
    if (scopes.size() > 1)
      scopes.pop_back();
  }

  bool define(SymbolKind kind, SymbolInfo info) {
    switch (kind) {
    case VAR:
      return defineVar(info.name, info.type, info.isConst);
    case FUNC:
      return defineFunc(info.name, info.type, info.paramTypes);
    case CLASS:
      return defineClass(info.name);
    case ARRAY:
      return defineArray(info.name, info.type, info.index, info.isConst);
      case OBJECT:
      return defineObject(info.name, info.type,info.isConst);
    }
    return false;
  }

  bool defineArray(const std::string &name, const std::string &type,
                   Expr::Ptr index, const bool isConst) {
    auto &current = scopes.back();
    /* for (auto const &n : current.vars)
      std::cout << n.first << "\n";*/
    if (current.vars.count(name))
      return false;
    current.vars[name] = std::make_shared<ArraySymbol>(
        name, type, isConst, scopes.size() - 1, index);
    return true;
  }

  // ────────── 변수 정의 ──────────
  bool defineVar(const std::string &name, const std::string &type,
                 bool isConst = false) {
    auto &current = scopes.back();
    if (current.vars.count(name))
      return false;
    current.vars[name] =
        std::make_shared<VarSymbol>(name, type, isConst, scopes.size() - 1);
    return true;
  }

  // ────────── 함수 정의 ──────────
  bool defineFunc(const std::string &name, const std::string &returnType,
                  const std::vector<std::string> &params) {
    auto &current = scopes.back();
    if (current.funcs.count(name))
      return false;
    current.funcs[name] = std::make_shared<FuncSymbol>(name, returnType, params,
                                                       scopes.size() - 1);
    return true;
  }

  bool defineClass(const std::string &name) {
    auto &current = scopes.back();
    if (current.classes.count(name))
      return false;
    current.classes[name] = std::make_shared<ClassSymbol>(name);
    return true;
  }

  bool defineObject(const std::string &name,const std::string className,const bool &  isConst){
    auto current=scopes.back();
    if(current.vars.count(name)) return false;
    current.vars[name]=std::make_shared<ObjectSymbol>(name,className,isConst,scopes.size()-1);
    return true;
  }


  // ────────── 변수 조회 ──────────
  VarSymbol *lookupVar(const std::string &name) {
    for (int i = scopes.size() - 1; i >= 0; --i) {
      auto it = scopes[i].vars.find(name);
      if (it != scopes[i].vars.end())
        return it->second.get();
    }
    return nullptr;
  }

  // ────────── 함수 조회 ──────────
  FuncSymbol *lookupFunc(const std::string &name) {
    for (int i = scopes.size() - 1; i >= 0; --i) {
      auto it = scopes[i].funcs.find(name);
      if (it != scopes[i].funcs.end())
        return it->second.get();
    }
    return nullptr;
  }

  ClassSymbol *lookupClass(const std::string &name) {
    for (int i = scopes.size() - 1; i >= 0; --i) {
      auto it = scopes[i].classes.find(name);
      if (it != scopes[i].classes.end())
        return it->second.get();
    }
    return nullptr;
  }

  Symbol *lookup(const std::string &name, SymbolKind kind) {
    switch (kind) {
    case ARRAY:
    case VAR:
    case OBJECT:
      return lookupVar(name);
    case FUNC:
      return lookupFunc(name);
    case CLASS:
      return lookupClass(name);    
    }
    return nullptr;
  }

  bool existsInCurrentScope(const std::string &name, SymbolKind kind) {
    auto &current = scopes.back();
    switch (kind) {
    case VAR:
    case OBJECT:
    case ARRAY:
      return current.vars.count(name);
    case FUNC:
      return current.funcs.count(name);
    case CLASS:
      return current.classes.count(name);

    }
    return false;
  }

  bool hasType(const std::string &name){
    for (int i = scopes.size() - 1; i >= 0; --i) {
      auto it = scopes[i].classes.find(name);
      if (it != scopes[i].classes.end())
        return true;
    }
    //TODO: 구조체 만들면 추가하기

    return false;
  }

  ClassSymbol *getType(std::string &name){
    for (int i = scopes.size() - 1; i >= 0; --i) {
      auto it = scopes[i].classes.find(name);
      if (it != scopes[i].classes.end())
        return it->second.get();
    }
    return nullptr;
  }  

  
};
