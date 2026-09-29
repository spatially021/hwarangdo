#include "hrd/Color.h"
#include "hrd/IR/HIR/HIRVerifier.h"
#include "hrd/Package/PackageWriter.h"
#include "hrd/compiler/CompilerDriver.h"
#include <iostream>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <magic_enum/magic_enum.hpp>

LibraryArtifacts CompilerDriver::makeLibraryArtifacts() const {
  const auto &name = projectInput.config.package.name;

  const fs::path buildRoot =
      projectInput.rootPath / CompilerConfig::BuildDirectory;

  const fs::path objectRoot = buildRoot / CompilerConfig::ObjectDirectory;

  const fs::path libraryRoot = buildRoot / CompilerConfig::LibraryDirectory;

  return {
      objectRoot / (name + std::string(CompilerConfig::ObjectExtension)),
      objectRoot / (name + std::string(CompilerConfig::MetaExtension)),
      objectRoot / (name + std::string(CompilerConfig::MIRExtension)),
      libraryRoot / (name + std::string(CompilerConfig::PackageExtension)),
  };
}

bool CompilerDriver::writeLibraryArtifacts(const LibraryArtifacts &artifacts) {

  if (!writeMeta(artifacts.meta)) {
    return false;
  }

  if (!writeMIR(artifacts.mir)) {
    return false;
  }

  return true;
}

bool CompilerDriver::packageLibrary(const LibraryArtifacts &artifacts) {

  PackageWriteInput input{
      projectInput.config.package,
      artifacts.object,
      artifacts.meta,
      artifacts.mir,
      artifacts.package,
  };

  PackageWriter writer;
  return writer.write(input);
}

void CompilerDriver::cleanupPackageObjects() {
#if !HRD_DEBUG
  const fs::path tempPath =
      projectInput.rootPath / "build" / "tmp" / "packages";

  std::error_code ec;

  fs::remove_all(tempPath, ec);

  if (ec) {
    std::cerr << "warning: failed to remove temporary package objects: "
              << ec.message() << '\n';
  }
#endif
}