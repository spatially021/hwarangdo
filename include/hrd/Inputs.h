#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct SourcePath {
  std::vector<std::string> segments;

  bool operator==(const SourcePath &s) const { return segments == s.segments; }
};

struct InputSource {
  std::string path;
  SourcePath logicalPath;
  std::string text;
};

enum class ModuleConfigStatus {
  Loaded,
  Missing,
  Invalid,
};

struct ModuleConfigResult {
  ModuleConfigStatus status;
  std::string name;
  std::string errorMessage;
};

struct ProjectInput {
  std::filesystem::path rootPath;
  std::filesystem::path projectFilePath;
  std::filesystem::path srcPath;
  std::vector<InputSource> sources;
  ModuleConfigResult config;
};
