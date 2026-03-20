#pragma once

#include "IR/HIR/HIRDecl.h"
#include "IR/HIR/HIRNode.h"
#include "IR/HIR/HIRType.h"
#include <memory>
#include <string>
#include <vector>
struct HIRModule : HIRNode {
  std::string name;

  std::vector<std::unique_ptr<HIRType>> ownedTypes;
  std::vector<std::unique_ptr<HIRTypeDecl>> typeDecls;
  std::vector<std::unique_ptr<HIRMethodDecl>> methodDecls;

  HIRVoidType *voidType = nullptr;
  HIRErrorType *errorType = nullptr;

  HIRModule() : HIRNode(HIRNodeKind::Module) {}
};