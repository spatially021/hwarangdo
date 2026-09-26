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
  ValueSymbol *buildField(FieldMeta &meta, ObjectType *symbol);
  void buildMethod(MethodMeta &meta, TypeSymbol *symbol);
  void buildParam(ParamMeta &meta, MethodSymbol *symbol, Scope *scope);
  void buildVariant(EnumVariantMeta &meta, EnumType *type, uint32_t ordinal);
  void buildTrait(TraitMeta &meta);
  void buildTraitMethod(MethodMeta &meta, TraitType *trait);

private:
  void link();
  void linkType(TypeMeta &meta);
  void linkField(FieldMeta &meta);
  void linkMethod(MethodMeta &meta, TypeSymbol *owner);
  void linkVariant(EnumVariantMeta &meta);
  void linkTrait(TraitMeta &meta);
  void linkTraitMethod(MethodMeta &meta, TraitType *trait);

private:
  void resolve();
  void resolveMethod(MethodMeta &meta, TypeSymbol *onwer);
  Expr::Ptr resolveDefault(DefaultValueMeta &meta, MethodSymbol *method);
  MethodSymbol *resolveInit(TypeSymbol *type,
                            std::vector<DefaultValueMeta> &args,
                            MethodSymbol *method);
  void resolveTrait(TraitMeta &meta);

  ArgMatchKind matchArgument(TypeSymbol *from, TypeSymbol *to);

private:
  void load();
  void loadType(const SourcePath &path, const string &name);

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
  unordered_map<TypeSymbol *,
                unordered_map<MethodMeta, MethodSymbol *, MethodHash>>
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
  TypeSymbol *getOrCreateTypeRef(TypeRef &ref, MethodSymbol *symbol = nullptr);
  ValueSymbol *getField(FieldMeta &meta);
  MethodSymbol *getMethod(TypeSymbol *owner, MethodMeta &ref);
  EnumVariantSymbol *getVariant(EnumVariantMeta &meta);
  TraitType *getTraitSymbol(TraitMeta &meta);
  TypeSymbol *substituteOwnerType(TypeSymbol *type, GenericSymbol *owner);
};