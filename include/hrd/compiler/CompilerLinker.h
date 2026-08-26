#pragma once

#include "hrd/compiler/LinkInput.h"

class CompilerLinker {
public:
  bool link(const LinkInput &input);
};