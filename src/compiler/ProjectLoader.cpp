#include "hrd/compiler/ProjectLoader.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

bool ProjectLoader::readTextFile(const fs::path &path, std::string &output) {
  std::ifstream file(path, std::ios::binary);

  if (!file) {
    return false;
  }

  std::ostringstream buffer;
  buffer << file.rdbuf();
  output = std::move(buffer).str();
  return true;
}

bool ProjectLoader::isProjectFile(const fs::path &path) {
  return path.extension() == ".toml";
}

bool ProjectLoader::isSourceFile(const fs::path &path) {
  return path.extension() == ".hrd";
}

std::optional<fs::path>
ProjectLoader::findProjectFile(const fs::path &projectRoot) {
  if (!fs::exists(projectRoot) || !fs::is_directory(projectRoot)) {
    std::cerr << "project directory does not exist: " << projectRoot << '\n';
    return std::nullopt;
  }

  std::vector<fs::path> candidates;

  for (const auto &entry : fs::directory_iterator(projectRoot)) {
    if (entry.is_regular_file() && isProjectFile(entry.path())) {
      candidates.push_back(entry.path());
    }
  }

  if (candidates.empty()) {
    std::cerr << "project file was not found: " << projectRoot << '\n';
    return std::nullopt;
  }

  if (candidates.size() != 1) {
    std::cerr << "multiple project files were found:\n";

    for (const auto &candidate : candidates) {
      std::cerr << "  - " << candidate << '\n';
    }

    return std::nullopt;
  }

  return candidates.front();
}

std::optional<fs::path>
ProjectLoader::findSourceDirectory(const fs::path &projectRoot) {
  const fs::path sourceRoot = projectRoot / "src";

  if (!fs::exists(sourceRoot) || !fs::is_directory(sourceRoot)) {
    std::cerr << "source directory was not found: " << sourceRoot << '\n';
    return std::nullopt;
  }

  return sourceRoot;
}

bool ProjectLoader::collectSources(const fs::path &sourceRoot,
                                   std::vector<InputSource> &sources) {
  std::vector<fs::path> paths;

  for (const auto &entry : fs::recursive_directory_iterator(sourceRoot)) {
    if (entry.is_regular_file() && isSourceFile(entry.path())) {
      paths.push_back(entry.path());
    }
  }

  if (paths.empty()) {
    std::cerr << "no source files found: " << sourceRoot << '\n';
    return false;
  }

  std::sort(paths.begin(), paths.end());

  sources.clear();
  sources.reserve(paths.size());

  for (const auto &path : paths) {
    InputSource source;
    source.path = path.lexically_normal().string();

    if (!readTextFile(path, source.text)) {
      std::cerr << "failed to read source file: " << path << '\n';
      return false;
    }

    sources.push_back(std::move(source));
  }

  return true;
}

std::optional<ProjectInput> ProjectLoader::load(const fs::path &projectRoot) {
  const auto projectFile = findProjectFile(projectRoot);

  if (!projectFile) {
    return std::nullopt;
  }

  const auto sourceRoot = findSourceDirectory(projectRoot);

  if (!sourceRoot) {
    return std::nullopt;
  }

  ProjectInput input;
  input.rootPath = projectRoot.string();
  input.projectFilePath = projectFile->string();
  input.srcPath = sourceRoot->string();

  if (!collectSources(*sourceRoot, input.sources)) {
    return std::nullopt;
  }

  return input;
}