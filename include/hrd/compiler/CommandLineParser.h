#pragma once

#include "CompilerInvocation.h"
#include <optional>

enum class FailKind {
  None,
  Help,
  Unknown,
};

struct LPresult {
  bool result = false;
  std::optional<CompilerInvocation> invocation;
  FailKind kind;
};

class CommandLineParser {
public:
  static LPresult parse(int argc, char *argv[]);
};