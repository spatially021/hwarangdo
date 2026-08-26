#pragma once

#include "hrd/Inputs.h"
#include "hrd/SemanticAnalyzer/Scope.h"
#include <filesystem>
#include <memory>
#include <unordered_map>

struct FileContext;

struct Module {
  std::string name;
  std::filesystem::path rootPath;
  Module(const string &n, std::filesystem::path p) : name(n), rootPath(p) {}
  vector<std::unique_ptr<FileContext>> files;
};

struct FileContext {
  Module *module;
  SourcePath path;
  FileContext(Module *m, SourcePath p) : module(m), path(p) {}
  std::unordered_map<std::string, TypeSymbol *> importedTypes;
};
