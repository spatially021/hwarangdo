#pragma once

#include "Node.h"
#include <memory>
#include <vector>

class ASTVisitor;

class ASTNode {

public:
  NodeKind kind;
  int line, col;

  ASTNode(NodeKind kind, int line = 0, int col = 0)
      : kind(kind), line(line), col(col) {};

  virtual ~ASTNode() = default;
  virtual void accept(ASTVisitor *visitor) = 0;

protected:
  virtual std::vector<std::shared_ptr<ASTNode>> getChildren() const {
    return {};
  }
};