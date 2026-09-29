#pragma once

#include "hrd/compiler/CompilerStruct.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <miniz.h>

namespace fs = std::filesystem;

class PackageReader {
public:
  PackageReader() = default;
  ~PackageReader();

  PackageReader(const PackageReader &) = delete;
  PackageReader &operator=(const PackageReader &) = delete;

  bool open(const fs::path &packagePath);
  void close();

  bool isOpen() const;

  std::optional<PackageManifest> readManifest() const;

  std::optional<std::vector<std::byte>>
  readMeta(const std::string &moduleName) const;

  std::optional<std::vector<std::byte>>
  readMIR(const std::string &moduleName) const;

  bool extractObject(const std::string &moduleName,
                     const fs::path &outputPath) const;

private:
  std::optional<std::vector<std::byte>>
  readEntry(const std::string &entryName) const;

  bool hasEntry(const std::string &entryName) const;

  mutable mz_zip_archive archive{};
  fs::path packagePath;
  bool opened = false;
};