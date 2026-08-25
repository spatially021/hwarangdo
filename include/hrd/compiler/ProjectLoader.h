#pragma once

#include "hrd/Inputs.h"

#include <filesystem>
#include <optional>

namespace fs = std::filesystem;

class ProjectLoader {
public:
  static std::optional<ProjectInput>
  load(const std::filesystem::path &projectRoot);

private:
  static std::optional<std::filesystem::path>
  findProjectFile(const std::filesystem::path &projectRoot);

  static std::optional<std::filesystem::path>
  findSourceDirectory(const std::filesystem::path &projectRoot);

  static bool collectSources(const std::filesystem::path &sourceRoot,
                             std::vector<InputSource> &sources);

  static bool readTextFile(const std::filesystem::path &path,
                           std::string &output);

  static bool isProjectFile(const std::filesystem::path &path);
  static bool isSourceFile(const std::filesystem::path &path);
  static ModuleConfigResult
  loadModuleConfig(const std::filesystem::path &rootPath);
  static SourcePath makeSourcePath(const fs::path &path,
                                   const fs::path &sourceRoot);
};