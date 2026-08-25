#include "hrd/AST/Decl.h"
#include "hrd/Inputs.h"
#include "hrd/SemanticAnalyzer/Module.h"
#include "hrd/SemanticAnalyzer/SymbolTable/SymbolRegistry.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/util/Error.h"
#include <optional>
#include <string>
#include <unordered_map>

using TypeMap = std::unordered_map<string, TypeSymbol *>;
using str = const string &;

const vector<TypeSymbol *> &SymbolRegistry::getDecledTypes() {
  return decledTypes;
}

const vector<TypeSymbol *> &SymbolRegistry::getTypes() { return typeRaw; }

ImplSymbol *SymbolRegistry::getImpl(ImplDecl *decl) {
  auto it = implMap.find(decl);
  if (it == implMap.end()) {
    Error::internal(decl->span, "fail to find implSymbol");
  }
  return it->second;
}

optional<RuntimeNamespace> SymbolRegistry::getRuntime(str name) {
  auto it = runtimeMap.find(name);
  if (it == runtimeMap.end()) {
    return nullopt;
  }
  return it->second;
}

TypeSymbol *SymbolRegistry::getType(str name) {

  auto itB = builtIn.find(name);
  if (itB != builtIn.end()) {
    return itB->second;
  }

  auto map = getTypeMap(currentFile);
  auto itT = map.find(name);
  if (itT != map.end()) {
    return itT->second;
  }

  auto itI = getImportedType(name);

  return itI;
}

TypeSymbol *SymbolRegistry::getBuilt(str name) { return builtIn.at(name); }

FileContext *SymbolRegistry::getFile(const SourcePath &path) {
  auto it = fileMap[currentModule].find(path);
  if (it == fileMap[currentModule].end()) {
    return nullptr;
  }
  return it->second;
}

FileContext *SymbolRegistry::getFile(Module *module, const SourcePath &path) {
  auto it = fileMap[module].find(path);
  if (it == fileMap[module].end()) {
    return nullptr;
  }
  return it->second;
}

TypeMap &SymbolRegistry::getTypeMap(FileContext *path) {
  auto it = typeMap.find(path);
  if (it == typeMap.end()) {
    Error::internal("fail to find typeMap");
  }
  return it->second;
}

TypeSymbol *SymbolRegistry::getImportedType(FileContext *path, str name) {
  auto it = path->importedTypes.find(name);
  if (it == path->importedTypes.end()) {
    return nullptr;
  }
  return it->second;
}

TypeSymbol *SymbolRegistry::getImportedType(str name) {
  auto it = currentFile->importedTypes.find(name);
  if (it == currentFile->importedTypes.end()) {
    return nullptr;
  }
  return it->second;
}

TypeSymbol *SymbolRegistry::findType(FileContext *path, str name) {
  auto map = getTypeMap(path);
  auto it = map.find(name);
  if (it != map.end()) {
    return it->second;
  }

  return getImportedType(path, name);
}

TypeMap &SymbolRegistry::findTypes(FileContext *path) {
  return getTypeMap(path);
}

FileContext *SymbolRegistry::getCurrentFile() { return currentFile; }

Module *SymbolRegistry::getModule(str name) {
  auto it = moduleMap.find(name);
  if (it == moduleMap.end()) {
    return nullptr;
  }
  return it->second;
}

Module *SymbolRegistry::getModule(const StringDatum &name) {
  return getModule(name.str);
}

RootSymbol *SymbolRegistry::getRoot() { return root.get(); }