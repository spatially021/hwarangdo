#include "hrd/compiler/ProjectLoader.h"
#include "hrd/compiler/CompilerStruct.h"

#include "hrd/Inputs.h"

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

  output = buffer.str();
  return true;
}

bool ProjectLoader::isSourceFile(const fs::path &path) {
  return path.extension() == ".hrd";
}

std::optional<fs::path>
ProjectLoader::findManifest(const fs::path &projectRoot) {
  const fs::path manifest = projectRoot / CompilerConfig::ProjectManifest;

  if (!fs::exists(manifest) || !fs::is_regular_file(manifest)) {
    std::cerr << "project manifest was not found: " << manifest << '\n';

    return std::nullopt;
  }

  return manifest;
}

std::optional<fs::path>
ProjectLoader::findSourceRoot(const fs::path &projectRoot) {
  const fs::path sourceRoot = projectRoot / "src";

  if (!fs::exists(sourceRoot) || !fs::is_directory(sourceRoot)) {
    std::cerr << "source directory was not found: " << sourceRoot << '\n';

    return std::nullopt;
  }

  return sourceRoot;
}

bool ProjectLoader::loadConfig(const fs::path &manifestPath,
                               ProjectConfig &config) {
  try {
    const toml::table manifest = toml::parse_file(manifestPath.string());

    // Package
    const auto *package = manifest["package"].as_table();

    if (package == nullptr) {
      std::cerr << "invalid project manifest: [package] table is missing\n";
      return false;
    }

    const auto name = (*package)["name"].value<std::string>();
    const auto developer = (*package)["developer"].value<std::string>();
    const auto version = (*package)["version"].value<std::string>();

    if (!name || name->empty()) {
      std::cerr << "invalid project manifest: package.name is missing\n";
      return false;
    }

    if (!developer || developer->empty()) {
      std::cerr << "invalid project manifest: package.developer is missing\n";
      return false;
    }

    if (!version || version->empty()) {
      std::cerr << "invalid project manifest: package.version is missing\n";
      return false;
    }

    config.package.name = *name;
    config.package.developer = *developer;
    config.package.version = *version;

    return true;

  } catch (const toml::parse_error &error) {
    std::cerr << "invalid project manifest: " << error.description() << '\n';

    return false;
  }
}

bool ProjectLoader::collectSources(const fs::path &sourceRoot,
                                   std::vector<InputSource> &sources) {

  std::vector<fs::path> paths;

  for (const auto &entry : fs::recursive_directory_iterator(sourceRoot)) {

    if (entry.is_regular_file() && isSourceFile(entry.path())) {
      paths.push_back(entry.path().lexically_normal());
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

  const fs::path root = fs::absolute(projectRoot).lexically_normal();

  if (!fs::exists(root) || !fs::is_directory(root)) {
    std::cerr << "project directory does not exist: " << root << '\n';

    return std::nullopt;
  }

  const auto manifestPath = findManifest(root);

  if (!manifestPath) {
    return std::nullopt;
  }

  const auto sourceRoot = findSourceRoot(root);

  if (!sourceRoot) {
    return std::nullopt;
  }

  ProjectInput input;

  input.rootPath = root;
  input.manifestPath = *manifestPath;
  input.sourceRoot = *sourceRoot;

  if (!loadConfig(input.manifestPath, input.config)) {
    return std::nullopt;
  }

  if (!collectSources(input.sourceRoot, input.sources)) {
    return std::nullopt;
  }

  return input;
}