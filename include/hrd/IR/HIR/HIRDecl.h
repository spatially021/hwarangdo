#pragma once

#include "hrd/AST/Expr.h"
#include "hrd/IR/HIR/HIRExpr.h"
#include "hrd/IR/HIR/HIRNode.h"
#include "hrd/IR/HIR/HIRStmt.h"
#include "hrd/IR/HIR/HIRSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/enums/MethodKind.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct HIRDecl : HIRNode {
  explicit HIRDecl(SourceSpan s, HIRNodeKind k) : HIRNode(s, k) {}
  virtual ~HIRDecl() = default;
};

enum class HIRTypeDeclKind {
  Struct,
  Class,
  Enum,
  Trait,
};
struct HIRTypeDecl : HIRDecl {
  HIRTypeDeclKind typeDeclKind;
  std::string name;
  TypeSymbol *type = nullptr;
  HIRTypeDecl *base = nullptr;

  int nextFieldId = 0;
  int nextMethodID = 0;

  TypeSymbol *symbol = nullptr;

  std::vector<std::unique_ptr<HIRMethodDecl>> methods;
  std::unordered_map<MethodSymbol *, HIRMethodDecl *> methodMap;
  std::unordered_map<MethodSymbol *, HIRMethodDecl *> initMap;
  HIRMethodDecl *onDestroy = nullptr;

  std::unordered_map<ValueSymbol *, Expr *> defaultInit;
  std::unique_ptr<HIRBlockStmt> defaultInitBlock = nullptr;

  HIRTypeDecl(SourceSpan s, HIRTypeDeclKind dk, std::string n, TypeSymbol *ty,
              TypeSymbol *sym)
      : HIRDecl(s, HIRNodeKind::TypeDecl), typeDeclKind(dk), name(std::move(n)),
        type(ty), symbol(sym) {}
};

struct HIRMethodDecl : HIRDecl {
  HIRTypeDecl *owner = nullptr;
  std::unique_ptr<HIRBlockStmt> body = nullptr;

  std::vector<unique_ptr<HIRLocal>> locals;
  int nextLocalId = 0;
  int nextParamID = 0;

  int id = -1;
  std::string name;
  TypeSymbol *returnType = nullptr;
  MethodSymbol *symbol = nullptr;
  std::vector<std::unique_ptr<HIRParam>> params;
  unordered_map<ValueSymbol *, HIRParam *> paramMap;

  MethodKind methodKind;
  bool isStatic = false;
  bool isAsync = false;

  HIRMethodDecl(SourceSpan s, HIRTypeDecl *o, int i, const string &n,
                MethodSymbol *m)
      : HIRDecl(s, HIRNodeKind::MethodDecl), owner(o), id(i), name(n),
        symbol(m) {}

  void setParam(std::vector<std::unique_ptr<HIRParam>> pa) {
    if (!params.empty() || !paramMap.empty()) {
      Error::internal("HIRMethodDecl::setParam called twice");
    }

    params = std::move(pa);
    paramMap.reserve(params.size());

    for (auto &p : params) {
      if (p == nullptr) {
        Error::internal("HIRMethodDecl has null param");
      }
      if (p->symbol == nullptr) {
        Error::internal("HIRParam symbol is nullptr");
      }

      auto [it, inserted] = paramMap.emplace(p->symbol, p.get());
      if (!inserted) {
        Error::internal("duplicate param symbol in HIRMethodDecl");
      }
    }
  }
};
