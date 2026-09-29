#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

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

struct PackageConfig {
  std::string name;
  std::string developer;
  std::string version;
};

struct ProjectConfig {
  PackageConfig package;
};

struct ProjectInput {
  fs::path rootPath;
  fs::path manifestPath;
  fs::path sourceRoot;

  ProjectConfig config;
  std::vector<InputSource> sources;
};