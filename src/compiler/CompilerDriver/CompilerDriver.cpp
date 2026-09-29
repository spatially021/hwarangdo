#ifndef NDEBUG
#define HRD_DEBUG 1
#else
#define HRD_DEBUG 0
#endif

#include "hrd/compiler/CompilerDriver.h"
#include "hrd/Color.h"
#include "hrd/IR/HIR/HIRVerifier.h"
#include "hrd/MetaData/MetaBuilder.h"
#include "hrd/SemanticAnalyzer/Module.h"
#include "hrd/SemanticAnalyzer/SymbolTable/SymbolTable.h"
#include "hrd/Serialize/MIRSerializationAdapter.h"
#include "hrd/compiler/CompilerContexts.h"
#include "hrd/compiler/CompilerLinker.h"
#include "hrd/compiler/CompilerSDK.h"
#include "hrd/compiler/LinkInput.h"
#include "hrd/diagnostic/DiagnosticEngine.h"
#include "hrd/diagnostic/DiagnosticRenderer.h"
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <magic_enum/magic_enum.hpp>
#include <memory>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <utility>
#include <vector>
#if HRD_DEBUG
#include <llvm/Support/FileSystem.h>
#endif

using namespace std;
namespace fs = std::filesystem;
namespace {

void printStage(std::size_t current, std::size_t total, std::string_view name) {
#if !HRD_DEBUG
  std::cout << '\r' << "\033[2K";
#endif

  std::cout << '[' << current << '/' << total << "] " << name << "...";
  std::cout.flush();

#if HRD_DEBUG
  std::cout << '\n';
#endif
}

void printElapsed(std::chrono::steady_clock::time_point start) {
  const auto end = std::chrono::steady_clock::now();
  const std::chrono::duration<double> elapsed = end - start;

  std::cout << "Finished in " << std::fixed << std::setprecision(2)
            << elapsed.count() << "s\n";
}

} // namespace

CompilerDriver::CompilerDriver()
    : engine(DiagnosticEngine(
          make_unique<TerminalDiagnosticRenderer>(std::cout))) {}

int CompilerDriver::run(int argc, char **argv) {
  const auto buildStart = std::chrono::steady_clock::now();

  try {
    if (!loadInput(argc, argv)) {
      return 1;
    }
  } catch (std::runtime_error &e) {
#if HRD_DEBUG
    cout << Color::RED << "error occur while input process\n" << Color::RESET;
#endif
    return false;
  }
  auto displayPath = [&](const fs::path &path) {
    std::error_code ec;
    auto relative = fs::relative(path, fs::current_path(), ec);
    return ec ? path : relative;
  };
  if (projectInput.sources.empty()) {
    std::cerr << "로드된 소스가 없습니다." << std::endl;
    return 1;
  }

  const std::string &moduleName = projectInput.config.package.name;
  auto sdk = SDKLocator::locate();
  if (!sdk.has_value()) {
    return 1;
  }
  storage.sdk = sdk.value();

  storage.module = make_unique<Module>(moduleName, projectInput.manifestPath);
  storage.table.registry.setModule(storage.module.get());
  const std::size_t stageCount = invocation.options.isCompile ? 13 : 10;
  size_t current = 1;
  std::cout << "Building " << projectInput.config.package.name;

  if (!projectInput.config.package.version.empty()) {
    std::cout << ' ' << projectInput.config.package.version;
  }

  std::cout << '\n';
  std::cout << "  Target  : "
            << (invocation.options.isCompile ? "library" : "executable")
            << '\n';
  std::cout << "  Sources : " << projectInput.sources.size() << "\n\n";
  printStage(current++, stageCount, "Lexing");
  if (!runLexer()) {
    return 1;
  }

  printStage(current++, stageCount, "Parsing");
  if (!runParser()) {
    return 1;
  }

  printStage(current++, stageCount, "Loading libraries");
  if (!loadLib()) {
    return 1;
  }

  printStage(current++, stageCount, "Semantic analysis");
  if (!runSemantic()) {
    return 1;
  }

  storage.table.registry.setCurrentFile(nullptr);

  printStage(current++, stageCount, "Building HIR");
  if (!runHIR()) {
    return 1;
  }

  printStage(current++, stageCount, "Initialization check");
  if (!runInitCheck()) {
    return 1;
  }

  if (invocation.options.isCompile) {
    printStage(current++, stageCount, "Generating metadata");

    if (!runMeta()) {
      return 1;
    }
  }

  printStage(current++, stageCount, "Building MIR");
  if (!runMIR()) {
    return 1;
  }

  printStage(current++, stageCount, "Loading library MIR");
  if (!loadMIR()) {
    return 1;
  }
  const auto objectPath =
      projectInput.rootPath / "build" / "obj" / (moduleName + ".o");

  const auto metaPath =
      projectInput.rootPath / "build" / "obj" / (moduleName + ".hmeta");
  const auto mirPath =
      projectInput.rootPath / "build" / "obj" / (moduleName + ".hmir");

  printStage(current++, stageCount, "Code generation");
  if (!runCodegen(objectPath)) {
    return 1;
  }

  const LibraryArtifacts artifacts = makeLibraryArtifacts();

  if (invocation.options.isCompile) {
    printStage(current++, stageCount, "Generating library object");

    if (!runCodegen(artifacts.object)) {
      return 1;
    }

    printStage(current++, stageCount, "Writing library artifacts");

    if (!writeLibraryArtifacts(artifacts)) {
      return 1;
    }

    printStage(current++, stageCount, "Packaging");

    if (!packageLibrary(artifacts)) {
      return 1;
    }
  }

  if (!invocation.options.isCompile) {
    printStage(current++, stageCount, "Linking");

    LinkInput input;

    input.input = objectPath;
    input.runtime = storage.sdk.runtime;
    input.output = projectInput.rootPath / "build" / "bin" / moduleName;
    for (auto &[_, import] : importedModules) {
      input.libraries.push_back(import.objectPath);
    }
    const auto nativePath = projectInput.rootPath / "native";

    if (fs::exists(nativePath) && fs::is_directory(nativePath)) {
      for (const auto &entry : fs::directory_iterator(nativePath)) {
        if (!entry.is_regular_file()) {
          continue;
        }

        if (entry.path().extension() != ".o") {
          continue;
        }

        input.natives.push_back(entry.path());
      }
    }

    CompilerLinker linker;

    if (!linker.link(input)) {
      return 1;
    }
    std::error_code ec;

#if !HRD_DEBUG
    fs::remove(objectPath, ec);
#endif

    if (ec) {
#if HRD_DEBUG
      std::cerr << "warning: failed to remove temporary object: "
                << ec.message() << '\n';
#endif
    }

    std::cout << Color::GREEN << "\ndone\n" << Color::RESET;
    std::cout << "\nExecutable: " << displayPath(input.output).string() << '\n';
    printElapsed(buildStart);
  } else {
    std::cout << Color::GREEN << "\ndone\n" << Color::RESET;
    std::cout << "\nPackage: " << displayPath(artifacts.package).string()
              << '\n';
    printElapsed(buildStart);
  }
  std::cout << Color::RESET;
  return 0;
}
