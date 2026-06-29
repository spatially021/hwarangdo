#pragma once

#include "hrd/Token.h"
#include <string>
#include <vector>
struct TokenStream {
  std::string path;
  std::vector<Token> tokens;
};