#pragma once

#include "CompilerOptions.h"
#include <filesystem>

struct CompilerInvocation {
  std::filesystem::path projectRoot;
  CompilerOptions options;
};