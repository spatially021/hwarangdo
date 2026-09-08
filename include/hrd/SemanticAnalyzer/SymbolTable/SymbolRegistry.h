#pragma once

#include "hrd/AST/Decl.h"
#include "hrd/Inputs.h"
#include "hrd/SemanticAnalyzer/Module.h"
#include "hrd/SemanticAnalyzer/SymbolTable/Hash.h"
#include "hrd/SemanticAnalyzer/symbol/RuntimeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/SourceSpan.h"
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

struct RuntimeNamespace {
  string name;
  unordered_map<string, vector<RuntimeSymbol *>> functions;
  RuntimeNamespace(string n) : name(n) {}
};

class SymbolRegistry {
  // ------namespace---
  using str = const string &;
  using TypeMap = std::unordered_map<string, TypeSymbol *>;

public:
  SymbolRegistry();
  ~SymbolRegistry();

  // ------- adds ---------
  void addTemp(std::unique_ptr<ValueSymbol> tempSymbol);
  void addBuilt(unique_ptr<TypeSymbol> typeSymbol);
  pair<bool, SourceSpan> addType(unique_ptr<TypeSymbol> typeSymbol);
  void addRuntime(unique_ptr<RuntimeSymbol> runtimeSymbol);
  void addSelf(unique_ptr<ValueSymbol> selfSymbol);
  void addImpl(ImplDecl *key, unique_ptr<ImplSymbol> implSymbol);
  void addFile(SourcePath path, FileContext *file);
  void addFile(Module *module, SourcePath path, FileContext *file);
  void addModule(const string &name, Module *module);
  pair<bool, SourceSpan> addMethod(unique_ptr<MethodSymbol> methodSymbol);
  bool addInit(unique_ptr<MethodSymbol> methodSymbol);
  bool addOnDestroy(unique_ptr<MethodSymbol> methodSymbol);

  // ------- getOrCreates ---------

  GenericSymbol *getOrCreateGeneric(TypeSymbol *origin,
                                    std::vector<TypeSymbol *> args);
  ArrayTypeSymbol *getOrCreateArray(TypeSymbol *base, llvm::APInt size);

  // ------- gets ---------
  const vector<TypeSymbol *> &getDecledTypes();
  const vector<TypeSymbol *> &getTypes();
  ImplSymbol *getImpl(ImplDecl *implDecl);
  optional<RuntimeNamespace> getRuntime(str name);

  FileContext *getCurrentFile();

  TypeSymbol *getType(str name);
  TypeSymbol *getBuilt(str name);
  FileContext *getFile(const SourcePath &path);
  FileContext *getFile(Module *moduel, const SourcePath &path);
  Module *getModule(str name);
  Module *getModule(const StringDatum &name);
  RootSymbol *getRoot();

  // ---- finds -----
  TypeSymbol *findType(FileContext *path, str name);
  TypeMap &findTypes(FileContext *path);

  // ------built-in shortcut----
  HandleSymbol *getHandle();
  ResultSymbol *getResult();
  OptionSymbol *getOption();
  TypeSymbol *getBool();

  // ------- other ---------
  ValueSymbol *createPayload(TypeSymbol *type);
  void setCurrentFile(FileContext *file);
  void setModule(Module *moudle);
  TypeSymbol *makeRoot();
  TypeSymbol *&getCurrent() { return currentType; }

private:
  TypeMap &getTypeMap(FileContext *path);
  TypeSymbol *getImportedType(FileContext *path, str name);
  TypeSymbol *getImportedType(str name);

private:
  FileContext *currentFile = nullptr;
  Module *currentModule = nullptr;
  TypeSymbol *currentType = nullptr;

  unordered_map<std::string, Module *> moduleMap;

  vector<unique_ptr<TypeSymbol>> types;
  vector<TypeSymbol *> decledTypes;
  vector<TypeSymbol *> typeRaw;

  vector<unique_ptr<ImplSymbol>> impls;
  unordered_map<Decl *, ImplSymbol *> implMap;

  vector<unique_ptr<RuntimeSymbol>> runtimes;

  unordered_map<ArrayTypeKey, ArrayTypeSymbol *, ArrayTypeHash> arrayTypeMap;
  unordered_map<GenericInsKey, GenericSymbol *, GenericInsHash> genericInsSMap;
  unordered_map<FileContext *, TypeMap> typeMap;
  unordered_map<Module *, unordered_map<SourcePath, FileContext *, PathHash>>
      fileMap;
  unordered_map<string, RuntimeNamespace> runtimeMap;
  unordered_map<string, TypeSymbol *> builtIn;

  vector<unique_ptr<ValueSymbol>> tmps;
  vector<unique_ptr<ValueSymbol>> payloadSymbols;
  vector<unique_ptr<ValueSymbol>> selfSymbols;
  unique_ptr<RootSymbol> root = make_unique<RootSymbol>();
};