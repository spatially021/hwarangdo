#include "hrd/AST/Decl.h"
#include "hrd/Inputs.h"
#include "hrd/SemanticAnalyzer/Module.h"
#include "hrd/SemanticAnalyzer/SymbolTable/SymbolRegistry.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/util/Error.h"
#include <memory>
#include <utility>

void SymbolRegistry::addTemp(unique_ptr<ValueSymbol> symbol) {
  tmps.push_back(std::move(symbol));
}

void SymbolRegistry::addBuilt(unique_ptr<TypeSymbol> type) {
  auto raw = type.get();
  types.push_back(std::move(type));
  typeRaw.push_back(raw);
  builtIn.emplace(raw->name, raw);
}

bool SymbolRegistry::addType(unique_ptr<TypeSymbol> symbol) {
  auto raw = symbol.get();
  types.push_back(std::move(symbol));
  typeRaw.push_back(raw);
  decledTypes.push_back(raw);
  raw->path = currentFile->path;
  auto it = typeMap.find(currentFile);
  if (it == typeMap.end()) {
    Error::internal("unknown file");
  }
  return it->second.emplace(raw->name, raw).second;
}

void SymbolRegistry::addRuntime(unique_ptr<RuntimeSymbol> runtime) {
  RuntimeSymbol *raw = runtime.get();

  auto [nsIt, _] =
      runtimeMap.try_emplace(runtime->namespaceName, runtime->namespaceName);

  auto &overloads = nsIt->second.functions[runtime->name];

  runtimes.push_back(std::move(runtime));
  overloads.push_back(raw);
}

void SymbolRegistry::addSelf(unique_ptr<ValueSymbol> symbol) {
  selfSymbols.push_back(std::move(symbol));
}

void SymbolRegistry::addImpl(ImplDecl *decl, unique_ptr<ImplSymbol> symbol) {
  auto raw = symbol.get();
  impls.push_back(std::move(symbol));
  implMap.emplace(decl, raw);
}

void SymbolRegistry::addFile(SourcePath path, FileContext *file) {
  auto [_, result] = fileMap[currentModule].emplace(path, file);
  if (!result) {
    Error::internal("fail to add file");
  }
  if (!typeMap.emplace(file, TypeMap()).second) {
    Error::internal("fail to add typeMap");
  }
}

void SymbolRegistry::addFile(Module *m, SourcePath path, FileContext *file) {
  auto [_, result] = fileMap[m].emplace(path, file);
  if (!result) {
    Error::internal("fail to add file");
  }
  if (!typeMap.emplace(file, TypeMap()).second) {
    Error::internal("fail to add typeMap");
  }
}

void SymbolRegistry::addModule(str name, Module *module) {
  moduleMap.emplace(name, module);
}