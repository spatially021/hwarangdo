#pragma once

#include "hrd/Inputs.h"
#include "hrd/Token.h"
#include <memory>
#include <string>
#include <vector>
struct TokenStream {
  std::string path;
  SourcePath locgicalPath;
  std::vector<Token> tokens;
  std::vector<std::unique_ptr<Token>> syntheticTokens;
  const Token &makeSyntheticToken() {
    auto token = std::make_unique<Token>();
    Token &result = *token;

    syntheticTokens.push_back(std::move(token));
    return result;
  }
};