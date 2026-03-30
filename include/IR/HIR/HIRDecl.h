#pragma once

#include "IR/HIR/HIRExpr.h"
#include "IR/HIR/HIRNode.h"
#include "IR/HIR/HIRSymbol.h"
#include "IR/HIR/HIRType.h"
#include "SemanticAnalyzer/symbol/MethodSymbol.h"
#include "SemanticAnalyzer/symbol/ValueSymbol.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

struct HIRMethodDecl;

struct HIRDecl : HIRNode {
  explicit HIRDecl(HIRNodeKind k, SourceSpan s = {}) : HIRNode(k, s) {}
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
  HIRType *type = nullptr;

  int nextFieldId = 0;
  int nextMethodID = 0;

  std::vector<std::unique_ptr<HIRField>> fields;
  std::unordered_map<ValueSymbol *, HIRField *> fieldMap;
  std::vector<std::unique_ptr<HIRMethodDecl>> methods;

  // enum 전용
  std::vector<std::unique_ptr<HIREnumVariant>> enumVariants;
  std::unordered_map<EnumVariantSymbol *, HIREnumVariant *> enumVariantMap;

  HIRTypeDecl(HIRTypeDeclKind dk, std::string n, HIRType *ty, SourceSpan s = {})
      : HIRDecl(HIRNodeKind::TypeDecl, s), typeDeclKind(dk), name(std::move(n)),
        type(ty) {}
};
struct HIRMethodDecl : HIRDecl {
  HIRTypeDecl *owner = nullptr; // nullable for top-level func
  std::unique_ptr<HIRBlockStmt> body;
  std::vector<unique_ptr<HIRLocal>> locals;
  int nextLocalId = 0;
  int nextParamID = 0;

  int id = -1;
  std::string name;
  HIRType *returnType = nullptr;
  MethodSymbol *symbol = nullptr;
  std::vector<std::unique_ptr<HIRParam>> params;
  unordered_map<ValueSymbol *, HIRParam *> paramMap;

  bool isInit = false;
  bool isStatic = false;
  bool isAsync = false;

  HIRMethodDecl(HIRTypeDecl *o, int i, const string &n, MethodSymbol *m,
                SourceSpan s = {})
      : HIRDecl(HIRNodeKind::MethodDecl, s), owner(o), id(i), name(n),
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