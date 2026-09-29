#include "hrd/Package/PackageReader.h"

#include <cstring>
#include <iostream>
#include <string>

#include <toml++/toml.hpp>

PackageReader::~PackageReader() { close(); }

bool PackageReader::open(const fs::path &path) {
  close();

  if (!fs::exists(path) || !fs::is_regular_file(path)) {
    std::cerr << "package was not found: " << path << '\n';
    return false;
  }

  if (path.extension() != ".hlib") {
    std::cerr << "invalid package extension: " << path << '\n';
    return false;
  }

  archive = {};

  if (!mz_zip_reader_init_file(&archive, path.string().c_str(), 0)) {
    std::cerr << "failed to open package: " << path << '\n';
    return false;
  }

  packagePath = path;
  opened = true;

  return true;
}

void PackageReader::close() {
  if (!opened) {
    return;
  }

  mz_zip_reader_end(&archive);

  archive = {};
  packagePath.clear();
  opened = false;
}

bool PackageReader::isOpen() const { return opened; }

bool PackageReader::hasEntry(const std::string &entryName) const {
  if (!opened) {
    return false;
  }

  return mz_zip_reader_locate_file(&archive, entryName.c_str(), nullptr, 0) >=
         0;
}

std::optional<std::vector<std::byte>>
PackageReader::readEntry(const std::string &entryName) const {
  if (!opened) {
    std::cerr << "package is not open\n";
    return std::nullopt;
  }

  std::size_t size = 0;

  void *data =
      mz_zip_reader_extract_file_to_heap(&archive, entryName.c_str(), &size, 0);

  if (data == nullptr) {
    std::cerr << "failed to read package entry: " << entryName << " from "
              << packagePath << '\n';
    return std::nullopt;
  }

  std::vector<std::byte> result(size);

  if (size != 0) {
    std::memcpy(result.data(), data, size);
  }

  mz_free(data);

  return result;
}

std::optional<PackageManifest> PackageReader::readManifest() const {
  auto data = readEntry("manifest.toml");

  if (!data) {
    return std::nullopt;
  }

  try {
    const std::string text(reinterpret_cast<const char *>(data->data()),
                           data->size());

    const toml::table manifest = toml::parse(text);

    const auto *package = manifest["package"].as_table();

    if (package == nullptr) {
      std::cerr << "invalid package manifest: [package] table is missing\n";
      return std::nullopt;
    }

    const auto name = (*package)["name"].value<std::string>();

    const auto developer = (*package)["developer"].value<std::string>();

    const auto version = (*package)["version"].value<std::string>();

    if (!name || name->empty()) {
      std::cerr << "invalid package manifest: package.name is missing\n";
      return std::nullopt;
    }

    if (!developer || developer->empty()) {
      std::cerr << "invalid package manifest: package.developer is missing\n";
      return std::nullopt;
    }

    if (!version || version->empty()) {
      std::cerr << "invalid package manifest: package.version is missing\n";
      return std::nullopt;
    }

    PackageManifest result;

    result.package.name = *name;
    result.package.developer = *developer;
    result.package.version = *version;

    return result;

  } catch (const toml::parse_error &error) {
    std::cerr << "invalid package manifest: " << error.description() << '\n';

    return std::nullopt;
  }
}

std::optional<std::vector<std::byte>>
PackageReader::readMeta(const std::string &moduleName) const {
  return readEntry(moduleName + ".hmeta");
}

std::optional<std::vector<std::byte>>
PackageReader::readMIR(const std::string &moduleName) const {
  return readEntry(moduleName + ".hmir");
}

bool PackageReader::extractObject(const std::string &moduleName,
                                  const fs::path &outputPath) const {

  if (!opened) {
    std::cerr << "package is not open\n";
    return false;
  }

  const std::string entryName = moduleName + ".o";

  if (!hasEntry(entryName)) {
    std::cerr << "object file was not found in package: " << entryName << '\n';
    return false;
  }

  std::error_code ec;

  fs::create_directories(outputPath.parent_path(), ec);

  if (ec) {
    std::cerr << "failed to create package temporary directory: "
              << outputPath.parent_path() << ": " << ec.message() << '\n';
    return false;
  }

  if (!mz_zip_reader_extract_file_to_file(&archive, entryName.c_str(),
                                          outputPath.string().c_str(), 0)) {
    std::cerr << "failed to extract object file: " << entryName << '\n';
    return false;
  }

  return true;
}