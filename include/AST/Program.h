#pragma once

#include "AST/Decl.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>
class SourceFile {
public:
  std::string path;
  std::vector<Decl::Ptr> decls;
  SourceFile(const string &p, std::vector<Decl::Ptr> d)
      : path(p), decls(std::move(d)) {}
};

struct Program {
  std::vector<shared_ptr<SourceFile>> sources;
};