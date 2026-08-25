#include "hrd/Imported/ImportedSymbolBuilder.h"
#include "hrd/Inputs.h"
#include "hrd/MetaData/MetaData.h"
#include "hrd/MetaData/TypeRef.h"
#include "hrd/SemanticAnalyzer/Module.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"
#include "hrd/util/Error.h"
#include <memory>
#include <unordered_map>
#include <utility>

FileContext *ImportedSymbolBuilder::getOrCreateFile(const SourcePath &path) {

  auto it = fileMap.find(path);
  if (it == fileMap.end()) {
    unique_ptr<FileContext> file = make_unique<FileContext>(module, path);
    auto raw = file.get();
    module->files.push_back(std::move(file));
    fileMap.emplace(path, raw);
    return raw;
  }
  return it->second;
}

FileContext *ImportedSymbolBuilder::getFile(const SourcePath &path) {
  auto it = fileMap.find(path);
  if (it == fileMap.end()) {
    Error::internal("fail to get fileContext");
  }
  return it->second;
}

unordered_map<string, unique_ptr<TypeSymbol>> &
ImportedSymbolBuilder::getTypeMap(FileContext *file) {
  if (file == nullptr) {
    Error::internal("file is nullptr");
  }

  return typeMap[file];
}

TypeSymbol *ImportedSymbolBuilder::getTypeSymbol(FileContext *file, str name) {
  auto &map = getTypeMap(file);
  auto it = map.find(name);
  if (it == map.end()) {
    Error::internal("fail to get type : " + name);
  }
  return it->second.get();
}

TypeSymbol *ImportedSymbolBuilder::getOrCreateTypeRef(TypeRef &ref) {
  if (ref.kind == TypeRefKind::BuiltIn) {
    if (!ref.builtIn.has_value()) {
      Error::internal("built-in type has no type");
    }
    return table.getBuilt(ref.builtIn.value());
  }
  auto it = typeRefMap.find(ref);
  if (it == typeRefMap.end()) {
    auto file = getFile(ref.path);
    auto type = getTypeSymbol(file, ref.name);
    typeRefMap.emplace(ref, type);
    return type;
  }
  return it->second;
}

ValueSymbol *ImportedSymbolBuilder::getField(FieldMeta &ref) {
  auto it = fieldMap[*currnet].find(ref);
  if (it == fieldMap[*currnet].end()) {
    Error::internal("fail to find field");
  }
  return it->second;
}

MethodSymbol *ImportedSymbolBuilder::getMethod(MethodMeta &ref) {
  auto it = methodMap[*currnet].find(ref);
  if (it == methodMap[*currnet].end()) {
    Error::internal("fail to find method");
  }
  return it->second;
}

EnumVariantSymbol *ImportedSymbolBuilder::getVariant(EnumVariantMeta &ref) {
  auto it = variantMap[*currnet].find(ref);
  if (it == variantMap[*currnet].end()) {
    Error::internal("fail to find variant");
  }
  return it->second;
}