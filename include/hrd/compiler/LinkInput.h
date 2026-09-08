#pragma once

#include <filesystem>
#include <vector>

struct LinkInput {
  std::filesystem::path input;
  std::filesystem::path output;

  std::filesystem::path runtime;

  std::vector<std::filesystem::path> libraries;
  std::vector<std::filesystem::path> natives;
};