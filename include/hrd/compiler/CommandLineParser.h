#pragma once

#include "CompilerInvocation.h"
#include <optional>

class CommandLineParser {
public:
  static std::optional<CompilerInvocation> parse(int argc, char *argv[]);

private:
  static void enableAllDumps(CompilerOptions &options);
};