#pragma once

#include <filesystem>
#include <optional>

struct SDKPaths {
  std::filesystem::path root;
  std::filesystem::path runtime;
  std::filesystem::path include;
  std::filesystem::path manifest;

  std::string target;
};
class SDKLocator {
public:
  static std::optional<SDKPaths> locate();
};