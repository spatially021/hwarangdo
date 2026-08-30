#pragma once

#include "hrd/AST/Expr.h"
#include "hrd/Imported/Hash.h"
#include "hrd/InitChecker/InitSummary.h"
#include "hrd/Inputs.h"
#include "hrd/MetaData/MetaData.h"
#include "hrd/MetaData/TypeRef.h"
#include "hrd/SemanticAnalyzer/Module.h"
#include "hrd/SemanticAnalyzer/Resolver.h"
#include "hrd/SemanticAnalyzer/SymbolTable/SymbolTable.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/compiler/CompilerContexts.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class ImportedSymbolBuilder {
  using str = const string &;

public:
  ImportedSymbolBuilder(ImportedContext &context);
  ~ImportedSymbolBuilder();
  void run();

private:
  void build();
  void buildType(TypeMeta &meta);
  ValueSymbol *buildField(FieldMeta &meta, TypeSymbol *symbol);
  void buildMethod(MethodMeta &meta, TypeSymbol *symbol);
  void buildParam(ParamMeta &meta, MethodSymbol *symbol, Scope *scope);
  void buildVariant(EnumVariantMeta &meta, TypeSymbol *type);

private:
  void link();
  void linkType(TypeMeta &meta);
  void linkField(FieldMeta &meta);
  void linkMethod(MethodMeta &meta);
  void linkVariant(EnumVariantMeta &meta);

private:
  void resolve();
  void resolveMethod(MethodMeta &meta);
  Expr::Ptr resolveDefault(DefaultValueMeta &meta);
  MethodSymbol *resolveInit(TypeSymbol *type,
                            std::vector<DefaultValueMeta> &args);
  ArgMatchKind matchArgument(TypeSymbol *from, TypeSymbol *to);

private:
  void load();

private:
  ModuleMeta &meta;
  Module *module;
  SymbolTable &table;
  vector<shared_ptr<Expr>> &ast;
  TypeMeta *currnet;
  Scope *scope;
  InitSummary &summary;
  std::unordered_map<SourcePath, FileContext *, PathHash> fileMap;
  std::unordered_map<FileContext *,
                     unordered_map<std::string, unique_ptr<TypeSymbol>>>
      typeMap;
  unordered_map<TypeMeta, unordered_map<FieldMeta, ValueSymbol *, FieldHash>,
                TypeMetaHash>
      fieldMap;
  unordered_map<TypeMeta, unordered_map<MethodMeta, MethodSymbol *, MethodHash>,
                TypeMetaHash>
      methodMap;
  unordered_map<
      TypeMeta,
      unordered_map<EnumVariantMeta, EnumVariantSymbol *, EnumVariantHash>,
      TypeMetaHash>
      variantMap;
  unordered_map<TypeRef, TypeSymbol *, TypeRefHash> typeRefMap;

  FileContext *getOrCreateFile(const SourcePath &path);
  FileContext *getFile(const SourcePath &path);
  unordered_map<string, unique_ptr<TypeSymbol>> &getTypeMap(FileContext *file);
  TypeSymbol *getTypeSymbol(FileContext *file, str name);
  TypeSymbol *getOrCreateTypeRef(TypeRef &ref);
  ValueSymbol *getField(FieldMeta &meta);
  MethodSymbol *getMethod(MethodMeta &meta);
  EnumVariantSymbol *getVariant(EnumVariantMeta &meta);
};