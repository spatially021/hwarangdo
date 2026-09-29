#pragma once

#include "hrd/Inputs.h"
#include <filesystem>

namespace fs = std::filesystem;

struct PackageWriteInput {
  PackageConfig package;

  fs::path objectPath;
  fs::path metaPath;
  fs::path mirPath;

  fs::path outputPath;
};

class PackageWriter {
public:
  bool write(const PackageWriteInput &input);

private:
  bool validateInput(const PackageWriteInput &input) const;

  bool writeManifest(const PackageConfig &package, const fs::path &path) const;
};