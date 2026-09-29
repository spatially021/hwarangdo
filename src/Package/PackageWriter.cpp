#include "hrd/Package/PackageWriter.h"

#include <fstream>
#include <iostream>
#include <string>

#include <miniz.h>
#include <toml++/toml.hpp>

namespace {

bool addFileToArchive(mz_zip_archive &archive, const fs::path &source,
                      const std::string &archiveName) {
  return mz_zip_writer_add_file(&archive, archiveName.c_str(),
                                source.string().c_str(), nullptr, 0,
                                MZ_BEST_COMPRESSION) != 0;
}

} // namespace

bool PackageWriter::write(const PackageWriteInput &input) {
  if (!validateInput(input)) {
    return false;
  }

  const fs::path outputDirectory = input.outputPath.parent_path();

  std::error_code ec;
  fs::create_directories(outputDirectory, ec);

  if (ec) {
    std::cerr << "failed to create package output directory: "
              << outputDirectory << ": " << ec.message() << '\n';
    return false;
  }

  //
  // manifest.toml은 패키징 중에만 필요한 임시 파일이다.
  //
  const fs::path manifestPath = outputDirectory / ".hlib-manifest.tmp.toml";

  if (!writeManifest(input.package, manifestPath)) {
    return false;
  }

  mz_zip_archive archive{};
  bool archiveInitialized = false;

  auto cleanup = [&]() {
    if (archiveInitialized) {
      mz_zip_writer_end(&archive);
    }

    std::error_code removeError;
    fs::remove(manifestPath, removeError);
  };

  //
  // 기존 package가 있다면 새 package로 교체한다.
  //
  fs::remove(input.outputPath, ec);

  if (ec) {
    std::cerr << "failed to remove old package: " << input.outputPath << ": "
              << ec.message() << '\n';

    cleanup();
    return false;
  }

  if (!mz_zip_writer_init_file(&archive, input.outputPath.string().c_str(),
                               0)) {
    std::cerr << "failed to create package: " << input.outputPath << '\n';

    cleanup();
    return false;
  }

  archiveInitialized = true;

  //
  // .hlib
  // ├── manifest.toml
  // ├── <name>.o
  // ├── <name>.hmeta
  // └── <name>.hmir
  //

  if (!addFileToArchive(archive, manifestPath, "manifest.toml")) {
    std::cerr << "failed to add manifest to package\n";

    cleanup();
    return false;
  }

  if (!addFileToArchive(archive, input.objectPath,
                        input.objectPath.filename().string())) {
    std::cerr << "failed to add object file to package: " << input.objectPath
              << '\n';

    cleanup();
    return false;
  }

  if (!addFileToArchive(archive, input.metaPath,
                        input.metaPath.filename().string())) {
    std::cerr << "failed to add metadata to package: " << input.metaPath
              << '\n';

    cleanup();
    return false;
  }

  if (!addFileToArchive(archive, input.mirPath,
                        input.mirPath.filename().string())) {
    std::cerr << "failed to add MIR to package: " << input.mirPath << '\n';

    cleanup();
    return false;
  }

  if (!mz_zip_writer_finalize_archive(&archive)) {
    std::cerr << "failed to finalize package: " << input.outputPath << '\n';

    cleanup();
    return false;
  }

  cleanup();

  return true;
}

bool PackageWriter::validateInput(const PackageWriteInput &input) const {

  if (input.package.name.empty()) {
    std::cerr << "package name is empty\n";
    return false;
  }

  if (input.package.developer.empty()) {
    std::cerr << "package developer is empty\n";
    return false;
  }

  if (input.package.version.empty()) {
    std::cerr << "package version is empty\n";
    return false;
  }

  if (!fs::exists(input.objectPath) || !fs::is_regular_file(input.objectPath)) {
    std::cerr << "object file was not found: " << input.objectPath << '\n';
    return false;
  }

  if (!fs::exists(input.metaPath) || !fs::is_regular_file(input.metaPath)) {
    std::cerr << "metadata file was not found: " << input.metaPath << '\n';
    return false;
  }

  if (!fs::exists(input.mirPath) || !fs::is_regular_file(input.mirPath)) {
    std::cerr << "MIR file was not found: " << input.mirPath << '\n';
    return false;
  }

  return true;
}

bool PackageWriter::writeManifest(const PackageConfig &package,
                                  const fs::path &path) const {

  toml::table manifest{
      {
          "package",
          toml::table{
              {"name", package.name},
              {"developer", package.developer},
              {"version", package.version},
          },
      },
  };

  std::ofstream output(path);

  if (!output) {
    std::cerr << "failed to create package manifest: " << path << '\n';
    return false;
  }

  output << manifest;

  if (!output) {
    std::cerr << "failed to write package manifest: " << path << '\n';
    return false;
  }

  return true;
}