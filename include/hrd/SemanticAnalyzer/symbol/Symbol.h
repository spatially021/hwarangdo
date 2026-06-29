#pragma once

#include "hrd/AST/Expr.h"
#include <string>
#include <unordered_map>
#include <vector>

using std::string;
using std::unordered_map;
using std::vector;

class Symbol {
public:
  string name;
  enum class SymbolType {
    TYPE,
    VALUE,
    IMPL,
    METHOD,
    ENUM_VARIANT,
    MAIN,
  } type;
  virtual ~Symbol() = default;

protected:
  virtual void _anchor() = 0;
};
