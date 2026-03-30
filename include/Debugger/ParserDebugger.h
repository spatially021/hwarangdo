#pragma once

#include "AST/Visitor.h"
#include <cstddef>
#include <iostream>
#include <string>

class ParserDebugger : public ASTVisitor {
public:
  std::size_t depth = 0;
  std::string ident();

#define AST_NODE(T) void visit(T *node) override;
#include "../AST/ASTNodeList.def"
#undef AST_NODE

private:
  template <typename Container, typename Func>
  void join(const Container &c, const char *sep, Func f) {
    bool first = true;
    for (const auto &elem : c) {
      if (!first)
        std::cout << sep;
      f(elem);
      first = false;
    }
  }

  template <typename Container>
  void joinAccept(const Container &c, const char *sep) {
    join(c, sep, [this](const auto &elem) { elem->accept(this); });
  }
};