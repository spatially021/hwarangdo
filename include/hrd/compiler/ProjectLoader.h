#pragma once

#include "hrd/Inputs.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

class ProjectLoader {
public:
  std::optional<ProjectInput> load(const fs::path &projectRoot);

private:
  std::optional<fs::path> findManifest(const fs::path &projectRoot);
  std::optional<fs::path> findSourceRoot(const fs::path &projectRoot);

  bool loadConfig(const fs::path &manifestPath, ProjectConfig &config);

  bool collectSources(const fs::path &sourceRoot,
                      std::vector<InputSource> &sources);

  SourcePath makeSourcePath(const fs::path &path, const fs::path &sourceRoot);

  bool readTextFile(const fs::path &path, std::string &output);

  bool isSourceFile(const fs::path &path);
};