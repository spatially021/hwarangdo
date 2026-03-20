#pragma once

#include "IR/HIR/HIRExpr.h"
#include "IR/HIR/HIRNode.h"
#include "IR/HIR/HIRSymbol.h"
#include "IR/HIR/HIRType.h"
#include <memory>
#include <string>
#include <vector>
struct HIRDecl : HIRNode {
  explicit HIRDecl(HIRNodeKind k, SourceSpan s = {}) : HIRNode(k, s) {}
  virtual ~HIRDecl() = default;
};

enum class HIRTypeDeclKind {
  Struct,
  Class,
  Enum,
};

struct HIRTypeDecl : HIRDecl {
  HIRTypeDeclKind typeDeclKind;
  std::string name;
  HIRType *type = nullptr;

  std::vector<std::unique_ptr<HIRField>> fields;
  std::vector<std::unique_ptr<HIRMethod>> methods;

  HIRTypeDecl(HIRTypeDeclKind dk, std::string n, HIRType *ty, SourceSpan s = {})
      : HIRDecl(HIRNodeKind::TypeDecl, s), typeDeclKind(dk), name(std::move(n)),
        type(ty) {}
};

struct HIRMethodDecl : HIRDecl {
  HIRMethod *method = nullptr;
  HIRTypeDecl *owner = nullptr; // nullable for top-level func
  std::unique_ptr<HIRBlockStmt> body;

  HIRMethodDecl(HIRMethod *m, HIRTypeDecl *o, std::unique_ptr<HIRBlockStmt> b,
                SourceSpan s = {})
      : HIRDecl(HIRNodeKind::MethodDecl, s), method(m), owner(o),
        body(std::move(b)) {}
};