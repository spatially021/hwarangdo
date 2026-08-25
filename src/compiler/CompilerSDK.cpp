#include "hrd/compiler/CompilerSDK.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include <llvm/Support/JSON.h>

namespace fs = std::filesystem;

namespace {

std::optional<fs::path> getExecutablePath() {
#ifdef __linux__

  std::error_code ec;

  fs::path path = fs::read_symlink("/proc/self/exe", ec);

  if (ec) {
    return std::nullopt;
  }

  return path;

#else
#error "SDK executable path detection is not implemented for this platform"
#endif
}

} // namespace

std::optional<SDKPaths> SDKLocator::locate() {

  auto executable = getExecutablePath();

  if (!executable.has_value()) {
    std::cerr << "error: failed to locate hrd executable\n";
    return std::nullopt;
  }

  // <prefix>/bin/hrd
  const fs::path binDir = executable->parent_path();

  // <prefix>
  const fs::path root = binDir.parent_path();

  const fs::path manifest = root / "share" / "hwarangdo" / "sdk.json";

  if (!fs::exists(manifest)) {
    std::cerr << "error: HwarangDo SDK manifest was not found\n"
              << "path: " << manifest << '\n';

    return std::nullopt;
  }

  std::ifstream file(manifest);

  if (!file) {
    std::cerr << "error: failed to open HwarangDo SDK manifest\n";

    return std::nullopt;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();

  auto parsed = llvm::json::parse(buffer.str());

  if (!parsed) {
    std::cerr << "error: invalid HwarangDo SDK manifest\n";

    return std::nullopt;
  }

  auto *object = parsed->getAsObject();

  if (!object) {
    std::cerr << "error: invalid HwarangDo SDK manifest root\n";

    return std::nullopt;
  }

  auto target = object->getString("target");
  auto runtimeLibrary = object->getString("runtimeLibrary");
  auto *paths = object->getObject("paths");

  if (!target || !runtimeLibrary || !paths) {
    std::cerr << "error: incomplete HwarangDo SDK manifest\n";
    return std::nullopt;
  }

  auto runtimePath = paths->getString("runtime");
  auto includePath = paths->getString("include");

  if (!runtimePath || !includePath) {
    std::cerr << "error: incomplete HwarangDo SDK paths\n";
    return std::nullopt;
  }
  SDKPaths sdk;

  sdk.root = root;
  sdk.manifest = manifest;
  sdk.target = target->str();

  sdk.runtime = root / runtimePath->str() / runtimeLibrary->str();

  sdk.include = root / includePath->str();
  if (!fs::exists(sdk.runtime)) {
    std::cerr << "error: HwarangDo runtime library was not found\n"
              << "path: " << sdk.runtime << '\n';

    return std::nullopt;
  }

  return sdk;
}