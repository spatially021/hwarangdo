#pragma once

#include "../Inputs.h"

#include <filesystem>
#include <optional>

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
};