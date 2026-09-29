#include "hrd/Package/PackageReader.h"
#ifndef NDEBUG
#define HRD_DEBUG 1
#else
#define HRD_DEBUG 0
#endif

#include "hrd/Color.h"
#include "hrd/IR/HIR/HIRVerifier.h"
#include "hrd/IR/MIR/MIRProgram.h"
#include "hrd/MetaData/MetaReader.h"
#include "hrd/MetaData/MetaWriter.h"
#include "hrd/Parser.h"
#include "hrd/SemanticAnalyzer/Module.h"
#include "hrd/Serialize/MIRSerialization.h"
#include "hrd/Serialize/MIRSerializationAdapter.h"
#include "hrd/compiler/CompilerContexts.h"
#include "hrd/compiler/CompilerDriver.h"

#if HRD_DEBUG
#include <llvm/Support/FileSystem.h>
#endif

#include <fstream>
#include <iostream>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <magic_enum/magic_enum.hpp>
#include <memory>
#include <stdexcept>
#include <utility>

bool CompilerDriver::writeMeta(const std::filesystem::path &metaPath) {
  fs::create_directories(metaPath.parent_path());

  std::ofstream metaOut(metaPath);

  if (!metaOut) {
    Error::internal("failed to open metadata output: " + metaPath.string());
  }
  MetaWriter writer = MetaWriter();
  try {
    writer.write(storage.module->name, storage.moduleMeta, metaOut);

  } catch (const std::runtime_error &e) {
    std::cerr << e.what() << '\n';
    return false;
  } catch (const Failure &) {
    return false;
  }
  return true;
}

bool CompilerDriver::writeMIR(const std::filesystem::path &mirPath) {
  fs::create_directories(mirPath.parent_path());

  try {
    auto adapter = makeMIRSerializationAdapter(storage.table);

    MIRSerializer::writeFile(mirPath, *storage.mirProgram, adapter.adapter);
  } catch (const std::runtime_error &e) {
    std::cerr << e.what() << '\n';
    return false;
  }

  return true;
}

void CompilerDriver::readMeta(const std::filesystem::path &libPath) {
  loadedPackages.clear();

  if (!fs::exists(libPath)) {
    return;
  }

  if (!fs::is_directory(libPath)) {
    Error::internal("library path is not a directory: " + libPath.string());
  }

  const fs::path tempRoot =
      projectInput.rootPath / "build" / "tmp" / "packages";

  fs::create_directories(tempRoot);

  for (const auto &entry : fs::directory_iterator(libPath)) {
    if (!entry.is_regular_file()) {
      continue;
    }

    if (entry.path().extension() != ".hlib") {
      continue;
    }

    PackageReader package;

    if (!package.open(entry.path())) {
      Error::meta(entry.path().string(), "failed to open package");
    }

    auto manifest = package.readManifest();

    if (!manifest) {
      Error::meta(entry.path().string(), "failed to read package manifest");
    }

    const std::string &moduleName = manifest->package.name;

    auto metaData = package.readMeta(moduleName);

    if (!metaData) {
      Error::meta(entry.path().string(),
                  "metadata not found in package: " + moduleName);
    }

    const std::string metaText(reinterpret_cast<const char *>(metaData->data()),
                               metaData->size());

    std::istringstream metaInput(metaText);

    MetaReader reader;

    MetaReadResult result = reader.read(metaInput, entry.path());

#if HRD_DEBUG
    MetaWriter writer = MetaWriter(true);

    cout << "\n";
    writer.write(result.moduleName, result.meta, std::cout);
#endif

    //
    // manifest의 package name과 metadata의 module name은
    // 같은 package를 나타내므로 반드시 일치해야 한다.
    //
    if (result.moduleName != moduleName) {
      Error::meta(entry.path().string(),
                  "package name and metadata module name do not match");
    }

    //
    // Object만 linker 때문에 실제 파일로 추출한다.
    //
    const fs::path objectPath =
        tempRoot / (manifest->package.developer + "_" + manifest->package.name +
                    "_" + manifest->package.version + ".o");

    if (!package.extractObject(moduleName, objectPath)) {
      Error::meta(entry.path().string(),
                  "failed to extract object file for module: " + moduleName);
    }

    auto [it, inserted] =
        importedModules.emplace(result.moduleName, ImportedModule{
                                                       std::move(result.meta),
                                                       objectPath,
                                                   });

    if (!inserted) {
      Error::meta(entry.path().string(),
                  "duplicated metadata module: " + result.moduleName);
    }

    loadedPackages.push_back(LoadedPackage{
        std::move(*manifest),
        entry.path(),
    });
  }
}

bool CompilerDriver::loadMIR() {
  try {
    auto *program = storage.mirProgram.get();

#if HRD_DEBUG
    for (auto *type : storage.table.registry.getTypes()) {
      std::cout << "[typeRaw] "
                << (type->module ? type->module->name : "<null>")
                << "::" << type->name << '\n';
    }
#endif

    auto adapter = makeMIRSerializationAdapter(storage.table);

    for (const auto &loaded : loadedPackages) {
      PackageReader package;

      if (!package.open(loaded.packagePath)) {
        throw std::runtime_error("failed to open package: " +
                                 loaded.packagePath.string());
      }

      const std::string &moduleName = loaded.manifest.package.name;

      auto mirData = package.readMIR(moduleName);

      if (!mirData) {
        throw std::runtime_error("MIR not found in package: " +
                                 loaded.packagePath.string());
      }

      const std::string buffer(reinterpret_cast<const char *>(mirData->data()),
                               mirData->size());

      std::istringstream input(buffer, std::ios::in | std::ios::binary);

      auto mir = MIRDeserializer::read(input, adapter.adapter);

      for (auto &func : mir->genericOrigin) {
        program->genericOrigin.push_back(std::move(func));
      }

      for (auto &it : mir->genericMap) {
        auto [_, inserted] = program->genericMap.emplace(it.first, it.second);

        if (!inserted) {
          Error::internal("duplicated generic MIR owner");
        }
      }
    }

  } catch (const std::runtime_error &e) {
    std::cerr << e.what() << '\n';
    return false;
  }

  return true;
}