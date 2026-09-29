#pragma once

#include "CompilerOptions.h"
#include "hrd/Inputs.h"
#include "hrd/compiler/CompilerInvocation.h"
#include "hrd/compiler/CompilerStruct.h"
#include "hrd/diagnostic/DiagnosticEngine.h"
#include <unordered_map>

class CompilerDriver {
public:
  CompilerDriver();
  int run(int argc, char **argv);

private:
  CompilerOptions options;
  ProjectInput projectInput;
  CompilerStorage storage;
  DiagnosticEngine engine;
  CompilerInvocation invocation;
  std::vector<LoadedPackage> loadedPackages;

  std::unordered_map<std::string, ImportedModule> importedModules;

  LibraryArtifacts makeLibraryArtifacts() const;
  bool writeLibraryArtifacts(const LibraryArtifacts &artifacts);
  bool packageLibrary(const LibraryArtifacts &artifacts);
  void cleanupPackageObjects();

  bool loadInput(int argc, char **argv);
  bool parseOptions(int argc, char **argv);

  bool runLexer();
  bool runParser();
  bool loadLib();
  bool runSemantic();
  bool runMeta();
  bool runHIR();
  bool runInitCheck();
  bool runMIR();
  bool runCodegen(const std::filesystem::path &objectPath);
  bool linkExecutable();
  bool writeMeta(const std::filesystem::path &metaPath);
  bool writeMIR(const std::filesystem::path &mirPath);
  void readMeta(const std::filesystem::path &libPath);
  bool loadMIR();
};