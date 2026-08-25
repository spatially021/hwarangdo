#include "hrd/compiler/ProjectLoader.h"
#include "hrd/compiler/CompilerConfig.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <toml++/toml.hpp>
#include <vector>

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
  const fs::path sourceRoot =
      fs::absolute(projectRoot / "src").lexically_normal();

  if (!fs::exists(sourceRoot) || !fs::is_directory(sourceRoot)) {
    std::cerr << "source directory was not found: " << sourceRoot << '\n';
    return std::nullopt;
  }

  return sourceRoot;
}

bool ProjectLoader::collectSources(const fs::path &sourceRoot,
                                   std::vector<InputSource> &sources) {
  std::vector<fs::path> paths;

  const fs::path normalizedSourceRoot =
      fs::absolute(sourceRoot).lexically_normal();

  for (const auto &entry :
       fs::recursive_directory_iterator(normalizedSourceRoot)) {
    if (entry.is_regular_file() && isSourceFile(entry.path())) {
      paths.push_back(entry.path().lexically_normal());
    }
  }

  if (paths.empty()) {
    std::cerr << "no source files found: " << normalizedSourceRoot << '\n';
    return false;
  }

  std::sort(paths.begin(), paths.end());

  sources.clear();
  sources.reserve(paths.size());

  for (const auto &path : paths) {
    InputSource source;

    source.path = path;
    source.logicalPath = makeSourcePath(path, sourceRoot);

    if (!readTextFile(path, source.text)) {
      std::cerr << "failed to read source file: " << path << '\n';
      return false;
    }

    sources.push_back(std::move(source));
  }

  return true;
}

SourcePath ProjectLoader::makeSourcePath(const fs::path &path,
                                         const fs::path &sourceRoot) {
  const fs::path relative =
      path.lexically_relative(sourceRoot).replace_extension();

  SourcePath result;

  for (const auto &part : relative) {
    result.segments.push_back(part.string());
  }

  return result;
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
  input.config = loadModuleConfig(projectRoot);
  return input;
}

ModuleConfigResult
ProjectLoader::loadModuleConfig(const std::filesystem::path &rootPath) {
  const auto path = rootPath / CompilerConfig::MoudleConfig;

  if (!std::filesystem::exists(path)) {
    return {
        ModuleConfigStatus::Missing,
        "module",
        "",
    };
  }

  try {
    const toml::table config = toml::parse_file(path.string());

    const auto *module = config["module"].as_table();
    if (module == nullptr) {
      return {
          ModuleConfigStatus::Invalid,
          "module",
          "[module] table is missing",
      };
    }

    const auto name = (*module)["name"].value<std::string>();

    if (!name || name->empty()) {
      return {
          ModuleConfigStatus::Invalid,
          "module",
          "module.name is missing",
      };
    }

    return {
        ModuleConfigStatus::Loaded,
        *name,
        "",
    };
  } catch (const toml::parse_error &error) {
    return {
        ModuleConfigStatus::Invalid,
        "module",
        std::string(error.description()),
    };
  }
}