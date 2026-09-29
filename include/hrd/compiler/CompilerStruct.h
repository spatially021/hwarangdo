#pragma once

#include "hrd/AST/Program.h"
#include "hrd/AST/TokenStream.h"
#include "hrd/IR/HIR/HIRProgram.h"
#include "hrd/IR/MIR/MIRProgram.h"
#include "hrd/InitChecker/InitSummary.h"
#include "hrd/MetaData/ImportedModule.h"
#include "hrd/MetaData/MetaData.h"
#include "hrd/SemanticAnalyzer/Module.h"
#include "hrd/SemanticAnalyzer/SymbolTable/SymbolTable.h"
#include "hrd/compiler/CompilerSDK.h"
#include <memory>
#include <string_view>
#include <vector>

struct PackageManifest {
  PackageConfig package;

  // Resolver 수정 후 추가
  // std::vector<PackageDependency> dependencies;
};

struct LoadedPackage {
  PackageManifest manifest;
  std::filesystem::path packagePath;
};

struct CompilerStorage {
  SDKPaths sdk;
  std::vector<TokenStream> tokenStreams;
  unique_ptr<Program> program = nullptr;
  SymbolTable table = SymbolTable();
  unique_ptr<Module> module = nullptr;
  vector<unique_ptr<Module>> modules;
  std::unique_ptr<HIRProgram> hirProgram = nullptr;
  std::unique_ptr<MIRProgram> mirProgram = nullptr;
  ModuleMeta moduleMeta;
  std::vector<shared_ptr<Expr>> imported;
  unique_ptr<Scope> libTopLevel = make_unique<Scope>();
  InitSummary summary;
};

struct CompilerConfig {
  static constexpr std::string_view ProjectManifest = "hwarangdo.toml";

  static constexpr std::string_view SourceDirectory = "src";
  static constexpr std::string_view BuildDirectory = "build";
  static constexpr std::string_view ObjectDirectory = "obj";
  static constexpr std::string_view BinaryDirectory = "bin";
  static constexpr std::string_view LibraryDirectory = "lib";

  static constexpr std::string_view ObjectExtension = ".o";
  static constexpr std::string_view MetaExtension = ".hmeta";
  static constexpr std::string_view MIRExtension = ".hmir";
  static constexpr std::string_view PackageExtension = ".hlib";

  static constexpr std::string_view PackageManifest = "manifest.toml";
};

struct LibraryArtifacts {
  fs::path object;
  fs::path meta;
  fs::path mir;
  fs::path package;
};
