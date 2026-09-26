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

TypeSymbol *ImportedSymbolBuilder::getOrCreateTypeRef(TypeRef &ref,
                                                      MethodSymbol *method) {
  if (ref.kind == TypeRefKind::BuiltIn) {
    if (!ref.builtIn.has_value()) {
      Error::internal("built-in type has no type");
    }

    return table.getBuilt(*ref.builtIn);
  }

  if (ref.kind == TypeRefKind::GenericParam) {
    if (ref.name.empty() || !ref.path.segments.empty() || currnet == nullptr) {
      Error::internal("invalid imported generic parameter reference");
    }

    if (method != nullptr) {
      auto &params = method->getGenericParamMap();

      auto it = params.find(ref.name);

      if (it != params.end()) {
        return it->second;
      }
    }

    auto *owner = getTypeSymbol(getFile(currnet->path), currnet->name);

    auto &params = owner->getGenericParamMap();

    auto it = params.find(ref.name);

    if (it == params.end()) {
      Error::internal("unknown imported generic parameter: " + ref.name);
    }
    auto type = it->second;
    return type;
  }

  if (ref.kind == TypeRefKind::Generic) {
    if (ref.name.empty() || ref.args.empty()) {
      Error::internal("invalid imported generic type reference");
    }

    auto *origin = getTypeSymbol(getFile(ref.path), ref.name);

    if (origin->getGenericParams().size() != ref.args.size()) {
      Error::internal("imported generic type argument count mismatch: " +
                      ref.name);
    }

    vector<TypeSymbol *> args;
    args.reserve(ref.args.size());

    for (auto &arg : ref.args) {
      args.push_back(getOrCreateTypeRef(arg, method));
    }

    return table.registry.getOrCreateGeneric(origin, std::move(args));
  }

  if (ref.kind == TypeRefKind::Array) {
    if (ref.args.size() != 1 || !ref.arraySize.has_value()) {
      Error::internal("invalid imported array type reference");
    }

    auto *element = getOrCreateTypeRef(ref.args.front(), method);

    return table.registry.getOrCreateArray(element, *ref.arraySize);
  }

  if (ref.kind != TypeRefKind::Declared) {
    Error::internal("unknown imported type reference kind");
  }

  auto it = typeRefMap.find(ref);

  if (it == typeRefMap.end()) {
    auto *file = getFile(ref.path);
    auto *type = getTypeSymbol(file, ref.name);

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

MethodSymbol *ImportedSymbolBuilder::getMethod(TypeSymbol *owner,
                                               MethodMeta &ref) {
  auto ownerIt = methodMap.find(owner);

  if (ownerIt == methodMap.end()) {
    Error::internal("fail to find imported method owner");
  }

  auto it = ownerIt->second.find(ref);

  if (it == ownerIt->second.end()) {
    Error::internal("fail to find imported method");
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

TypeSymbol *ImportedSymbolBuilder::substituteOwnerType(TypeSymbol *type,
                                                       GenericSymbol *owner) {

  if (type == nullptr) {
    Error::internal("cannot substitute null imported type");
  }

  if (auto *param = dyn_cast<GenericParamSymbol>(type)) {

    if (param->owner == owner->origin) {
      if (param->index >= owner->args.size()) {
        Error::internal("imported generic parameter index out of range");
      }

      return owner->args[param->index];
    }

    return param;
  }

  if (auto *generic = dyn_cast<GenericSymbol>(type)) {

    vector<TypeSymbol *> args;
    args.reserve(generic->args.size());

    for (auto *arg : generic->args) {
      args.push_back(substituteOwnerType(arg, owner));
    }

    return table.registry.getOrCreateGeneric(generic->origin, std::move(args));
  }

  if (auto *array = dyn_cast<ArrayTypeSymbol>(type)) {

    auto *base = substituteOwnerType(array->baseType, owner);

    return table.registry.getOrCreateArray(base, array->sizeValue);
  }

  return type;
}

TraitType *ImportedSymbolBuilder::getTraitSymbol(TraitMeta &ref) {
  auto *file = getFile(ref.path);
  auto *type = getTypeSymbol(file, ref.name);

  auto *trait = dyn_cast<TraitType>(type);

  if (trait == nullptr) {
    Error::internal("imported type is not trait: " + ref.name);
  }

  return trait;
}