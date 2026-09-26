#include "hrd/Serialize/MIRSymbolIndex.h"

#include "hrd/SemanticAnalyzer/Module.h"
#include "hrd/SemanticAnalyzer/Scope.h"
#include "hrd/SemanticAnalyzer/SymbolTable/SymbolRegistry.h"
#include "hrd/SemanticAnalyzer/SymbolTable/SymbolTable.h"
#include "hrd/util/Helper.h"

#include <charconv>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <llvm/ADT/APInt.h>
#include <llvm/ADT/StringRef.h>

namespace {

[[noreturn]] void fail(const std::string &message) {
  throw std::runtime_error("MIR symbol index: " + message);
}

std::string pack(std::string_view value) {
  return std::to_string(value.size()) + "#" + std::string(value);
}

std::string joinPath(const SourcePath &path) {
  std::string result;
  for (std::size_t i = 0; i < path.segments.size(); ++i) {
    if (i != 0) {
      result += ".";
    }
    result += path.segments[i];
  }
  return result;
}

struct KeyReader {
  std::string_view value;
  std::size_t pos = 0;

  char takeChar() {
    if (pos >= value.size()) {
      fail("unexpected end of key");
    }
    return value[pos++];
  }

  std::string_view takePart() {
    const auto hash = value.find('#', pos);
    if (hash == std::string_view::npos) {
      fail("malformed packed key");
    }

    std::size_t size = 0;
    const auto number = value.substr(pos, hash - pos);
    const auto *begin = number.data();
    const auto *end = begin + number.size();
    auto [ptr, ec] = std::from_chars(begin, end, size);
    if (ec != std::errc() || ptr != end) {
      fail("invalid packed key length");
    }

    pos = hash + 1;
    if (size > value.size() - pos) {
      fail("packed key length exceeds input");
    }

    const auto result = value.substr(pos, size);
    pos += size;
    return result;
  }

  uint32_t takeU32Part() {
    const auto part = takePart();
    uint32_t result = 0;
    const auto *begin = part.data();
    const auto *end = begin + part.size();
    auto [ptr, ec] = std::from_chars(begin, end, result);
    if (ec != std::errc() || ptr != end) {
      fail("invalid u32 key part");
    }
    return result;
  }
};

bool isBuiltinLike(TypeSymbol *type) {
  if (type == nullptr) {
    return false;
  }

  if (type->kind == TypeKind::PRIMITIVE || type->kind == TypeKind::VOID ||
      type->kind == TypeKind::HANDLE || type->kind == TypeKind::RESULT ||
      type->kind == TypeKind::OPTION || type->kind == TypeKind::BUILTIN ||
      type->kind == TypeKind::DEFAULT_VALUE) {
    return type->module == nullptr;
  }

  return false;
}

} // namespace

MIRSymbolIndex::MIRSymbolIndex(SymbolTable &t)
    : table(t), registry(t.registry) {
  rebuild();
}

void MIRSymbolIndex::rebuild() {
  types.clear();
  methods.clear();
  fields.clear();
  variants.clear();
  genericParams.clear();

  typeKeys.clear();
  methodKeys.clear();
  fieldKeys.clear();
  variantKeys.clear();
  genericParamKeys.clear();

  indexedTypes.clear();
  indexedMethods.clear();

  for (auto *type : registry.getTypes()) {
    indexType(type);
  }

  // Root values are not ObjectType::fields. RootExpr lowers to MIRRootPlace +
  // MIRFieldPlace, so give those symbols a stable key as well.
  if (auto *rootScope = table.scopeManger.getRootScope()) {
    for (auto &[name, owner] : rootScope->value) {
      auto *field = owner.get();
      if (field == nullptr) {
        continue;
      }
      const std::string key = "R" + pack(name);
      fieldKeys.emplace(field, key);
      fields.emplace(key, field);
    }
  }

  // Generic declarations such as Handle<T> are intentionally omitted from
  // getTypes()/typeRaw. Index the currently active builtins when referenced by
  // makeTypeKey(); builtin resolution itself uses registry.getBuilt().
}

void MIRSymbolIndex::indexType(TypeSymbol *type) {
  if (type == nullptr || !indexedTypes.emplace(type).second) {
    return;
  }

  const auto key = makeTypeKey(type);
  typeKeys.emplace(type, key);
  types.emplace(key, type);

  indexGenericParams(*type, key, 'T');

  for (auto *method : type->getAllMethods()) {
    indexMethod(method);
  }

  if (auto *object = dyn_cast<ObjectType>(type)) {
    for (auto *field : object->fields) {
      if (field == nullptr) {
        continue;
      }
      const auto fieldKey = makeFieldKey(type, field);
      fieldKeys.emplace(field, fieldKey);
      fields.emplace(fieldKey, field);
    }

    for (auto *init : object->getInits()) {
      indexMethod(init);
    }

    if (object->onDestroy) {
      indexMethod(object->onDestroy.get());
    }
  }

  if (auto *enumType = dyn_cast<EnumType>(type)) {
    for (auto &variantOwner : enumType->variants) {
      auto *variant = variantOwner.get();
      const auto variantKey = makeVariantKey(enumType, variant);
      variantKeys.emplace(variant, variantKey);
      variants.emplace(variantKey, variant);
    }
  }
}

void MIRSymbolIndex::indexMethod(MethodSymbol *method) {
  if (method == nullptr || !indexedMethods.emplace(method).second) {
    return;
  }

  const auto key = makeMethodKey(method);
  methodKeys.emplace(method, key);
  methods.emplace(key, method);
  indexGenericParams(*method, key, 'M');
}

void MIRSymbolIndex::indexGenericParams(GenericOnwer &owner,
                                        std::string_view ownerKey,
                                        char ownerKind) {
  const auto &params = owner.getGenericParams();
  for (auto *param : params) {
    if (param == nullptr) {
      continue;
    }

    std::string key = "Q";
    key += ownerKind;
    key += pack(ownerKey);
    key += pack(std::to_string(param->index));

    genericParamKeys.emplace(param, key);
    genericParams.emplace(key, param);

    // GenericParamSymbol is itself a TypeSymbol and may appear as
    // MIRValue::type.
    const std::string typeKey = "P" + pack(key);
    typeKeys.emplace(param, typeKey);
    types.emplace(typeKey, param);
  }
}

std::string MIRSymbolIndex::makeTypeKey(TypeSymbol *symbol) {
  if (symbol == nullptr) {
    fail("cannot key null type");
  }

  if (auto it = typeKeys.find(symbol); it != typeKeys.end()) {
    return it->second;
  }

  if (auto *param = dyn_cast<GenericParamSymbol>(symbol)) {
    auto it = genericParamKeys.find(param);
    if (it == genericParamKeys.end()) {
      fail("generic parameter owner was not indexed: " + param->name);
    }
    return "P" + pack(it->second);
  }

  if (auto *generic = dyn_cast<GenericSymbol>(symbol)) {
    std::string key = "G";
    key += pack(typeKey(generic->origin));
    key += pack(std::to_string(generic->args.size()));
    for (auto *arg : generic->args) {
      key += pack(typeKey(arg));
    }
    return key;
  }

  if (auto *array = dyn_cast<ArrayTypeSymbol>(symbol)) {
    std::string key = "A";
    key += pack(typeKey(array->baseType));
    key += pack(std::to_string(array->sizeValue.getBitWidth()));
    key += pack(Helper::apIntToString(array->sizeValue));
    return key;
  }

  if (isBuiltinLike(symbol)) {
    return "B" + pack(symbol->name);
  }

  if (symbol->module == nullptr) {
    fail("non-builtin type has null module: " + symbol->name);
  }

  std::string key = "T";
  key += pack(symbol->module->name);
  key += pack(joinPath(symbol->path));
  key += pack(symbol->name);
  return key;
}

std::string MIRSymbolIndex::makeMethodKey(MethodSymbol *symbol) {
  if (symbol == nullptr) {
    fail("cannot key null method");
  }

  // A method key is needed before the method's own GenericParamSymbol objects
  // can be indexed. Therefore parameter types must encode generic parameters
  // owned by this method as local placeholders instead of calling typeKey(),
  // which would require genericParamKeys to already contain them.
  //
  // Example: foo<U>(List<U>)
  //   U       -> L0
  //   List<U> -> G(originKey, L0)
  //
  // Type generic parameters (e.g. class Box<T>) have already been indexed by
  // indexType() before its methods are visited, so those can use typeKey().
  std::function<std::string(TypeSymbol *)> signatureTypeKey;
  signatureTypeKey = [&](TypeSymbol *type) -> std::string {
    if (type == nullptr) {
      fail("method signature contains null type: " + symbol->name);
    }

    if (auto *param = dyn_cast<GenericParamSymbol>(type)) {
      if (param->owner == static_cast<GenericOnwer *>(symbol)) {
        std::string local = "L";
        local += pack(std::to_string(param->index));
        return local;
      }
      return typeKey(type);
    }

    if (auto *generic = dyn_cast<GenericSymbol>(type)) {
      std::string result = "G";
      result += pack(signatureTypeKey(generic->origin));
      result += pack(std::to_string(generic->args.size()));
      for (auto *arg : generic->args) {
        result += pack(signatureTypeKey(arg));
      }
      return result;
    }

    if (auto *array = dyn_cast<ArrayTypeSymbol>(type)) {
      std::string result = "A";
      result += pack(signatureTypeKey(array->baseType));
      result += pack(std::to_string(array->sizeValue.getBitWidth()));
      result += pack(Helper::apIntToString(array->sizeValue));
      return result;
    }

    return typeKey(type);
  };

  std::string key = "M";
  key += pack(symbol->module ? symbol->module->name : std::string{});
  key += pack(joinPath(symbol->path));
  key += pack(symbol->owner ? typeKey(symbol->owner) : std::string{});
  key += pack(symbol->name);
  key += pack(std::to_string(symbol->getGenericParams().size()));
  key += pack(std::to_string(symbol->params.size()));

  for (auto *param : symbol->params) {
    if (param == nullptr || param->typeSymbol == nullptr) {
      fail("method parameter has no type: " + symbol->name);
    }
    key += pack(signatureTypeKey(param->typeSymbol));
  }

  return key;
}

std::string MIRSymbolIndex::makeFieldKey(TypeSymbol *owner,
                                         ValueSymbol *field) const {
  if (owner == nullptr || field == nullptr) {
    fail("cannot key null field");
  }

  auto ownerIt = typeKeys.find(owner);
  if (ownerIt == typeKeys.end()) {
    fail("field owner type is not indexed");
  }

  std::string key = "F";
  key += pack(ownerIt->second);
  key += pack(field->name);
  key += pack(std::to_string(field->index));
  return key;
}

std::string MIRSymbolIndex::makeVariantKey(EnumType *owner,
                                           EnumVariantSymbol *variant) const {
  if (owner == nullptr || variant == nullptr) {
    fail("cannot key null enum variant");
  }

  auto ownerIt = typeKeys.find(owner);
  if (ownerIt == typeKeys.end()) {
    fail("enum owner type is not indexed");
  }

  std::string key = "V";
  key += pack(ownerIt->second);
  key += pack(variant->name);
  key += pack(std::to_string(variant->ordinal));
  return key;
}

std::string MIRSymbolIndex::typeKey(TypeSymbol *symbol) {
  auto it = typeKeys.find(symbol);
  if (it != typeKeys.end()) {
    return it->second;
  }

  const auto key = makeTypeKey(symbol);
  typeKeys.emplace(symbol, key);
  types.emplace(key, symbol);
  return key;
}

std::string MIRSymbolIndex::methodKey(MethodSymbol *symbol) {
  auto it = methodKeys.find(symbol);
  if (it != methodKeys.end()) {
    return it->second;
  }

  indexMethod(symbol);
  return methodKeys.at(symbol);
}

std::string MIRSymbolIndex::fieldKey(ValueSymbol *symbol) {
  auto it = fieldKeys.find(symbol);
  if (it != fieldKeys.end()) {
    return it->second;
  }

  if (symbol == nullptr) {
    fail("cannot key null field");
  }

  if (auto *owner = std::get_if<TypeSymbol *>(&symbol->owner)) {
    const auto key = makeFieldKey(*owner, symbol);
    fieldKeys.emplace(symbol, key);
    fields.emplace(key, symbol);
    return key;
  }

  if (symbol->isRoot) {
    const std::string key = "R" + pack(symbol->name);
    fieldKeys.emplace(symbol, key);
    fields.emplace(key, symbol);
    return key;
  }

  fail("MIR field is not owned by a type/root: " + symbol->name);
}

std::string MIRSymbolIndex::variantKey(EnumVariantSymbol *symbol) {
  auto it = variantKeys.find(symbol);
  if (it != variantKeys.end()) {
    return it->second;
  }

  if (symbol == nullptr) {
    fail("cannot key null enum variant");
  }

  auto *ownerPtr = std::get_if<TypeSymbol *>(&symbol->owner);
  if (ownerPtr == nullptr) {
    fail("enum variant has no TypeSymbol owner: " + symbol->name);
  }

  auto *enumType = dyn_cast<EnumType>(*ownerPtr);
  if (enumType == nullptr) {
    fail("enum variant owner is not EnumType: " + symbol->name);
  }

  const auto key = makeVariantKey(enumType, symbol);
  variantKeys.emplace(symbol, key);
  variants.emplace(key, symbol);
  return key;
}

std::string MIRSymbolIndex::runtimeKey(RuntimeSymbol *symbol) {
  if (symbol == nullptr) {
    fail("cannot key null runtime symbol");
  }

  std::string key = "N";
  key += pack(symbol->namespaceName);
  key += pack(symbol->name);
  key += pack(std::to_string(symbol->params.size()));
  for (auto *param : symbol->params) {
    key += pack(typeKey(param));
  }
  return key;
}

std::string MIRSymbolIndex::genericParamKey(GenericParamSymbol *symbol) {
  auto it = genericParamKeys.find(symbol);
  if (it == genericParamKeys.end()) {
    fail("generic parameter owner was not indexed: " +
         (symbol ? symbol->name : std::string("<null>")));
  }
  return it->second;
}

TypeSymbol *MIRSymbolIndex::resolveConstructedType(std::string_view key) {
  KeyReader reader{key};
  const char kind = reader.takeChar();

  if (kind == 'B') {
    const auto name = std::string(reader.takePart());
    return registry.getBuilt(name);
  }

  if (kind == 'G') {
    const auto originKey = reader.takePart();
    const auto countPart = reader.takePart();

    std::size_t count = 0;
    const auto *begin = countPart.data();
    const auto *end = begin + countPart.size();
    auto [ptr, ec] = std::from_chars(begin, end, count);
    if (ec != std::errc() || ptr != end) {
      fail("invalid generic argument count");
    }

    auto *origin = resolveType(originKey);
    std::vector<TypeSymbol *> args;
    args.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
      args.push_back(resolveType(reader.takePart()));
    }

    auto *result = registry.getOrCreateGeneric(origin, std::move(args));
    indexType(result);
    return result;
  }

  if (kind == 'A') {
    auto *base = resolveType(reader.takePart());
    const uint32_t bitWidth = reader.takeU32Part();
    const auto value = reader.takePart();

    llvm::APInt size(bitWidth, llvm::StringRef(value.data(), value.size()), 10);
    auto *result = registry.getOrCreateArray(base, std::move(size));
    indexType(result);
    return result;
  }

  if (kind == 'P') {
    const auto paramKey = reader.takePart();
    return resolveGenericParam(paramKey);
  }

  return nullptr;
}

TypeSymbol *MIRSymbolIndex::resolveType(std::string_view key) {
  if (auto it = types.find(std::string(key)); it != types.end()) {
    return it->second;
  }

  auto *result = resolveConstructedType(key);
  if (result != nullptr) {
    const std::string ownedKey(key);
    types.emplace(ownedKey, result);
    typeKeys.emplace(result, ownedKey);
  }
  return result;
}

MethodSymbol *MIRSymbolIndex::resolveMethod(std::string_view key) const {
  auto it = methods.find(std::string(key));
  return it == methods.end() ? nullptr : it->second;
}

ValueSymbol *MIRSymbolIndex::resolveField(std::string_view key) const {
  auto it = fields.find(std::string(key));
  return it == fields.end() ? nullptr : it->second;
}

EnumVariantSymbol *MIRSymbolIndex::resolveVariant(std::string_view key) const {
  auto it = variants.find(std::string(key));
  return it == variants.end() ? nullptr : it->second;
}

RuntimeSymbol *MIRSymbolIndex::resolveRuntime(std::string_view key) {
  KeyReader reader{key};
  if (reader.takeChar() != 'N') {
    return nullptr;
  }

  const std::string namespaceName(reader.takePart());
  const std::string functionName(reader.takePart());
  const auto countPart = reader.takePart();

  std::size_t count = 0;
  const auto *begin = countPart.data();
  const auto *end = begin + countPart.size();
  auto [ptr, ec] = std::from_chars(begin, end, count);
  if (ec != std::errc() || ptr != end) {
    fail("invalid runtime parameter count");
  }

  // Consume parameter keys here only to validate the key shape. Matching below
  // uses runtimeKey(candidate), keeping a single canonical implementation.
  for (std::size_t i = 0; i < count; ++i) {
    (void)reader.takePart();
  }

  auto ns = registry.getRuntime(namespaceName);
  if (!ns.has_value()) {
    return nullptr;
  }

  auto overloadIt = ns->functions.find(functionName);
  if (overloadIt == ns->functions.end()) {
    return nullptr;
  }

  for (auto *candidate : overloadIt->second) {
    if (runtimeKey(candidate) == key) {
      return candidate;
    }
  }

  return nullptr;
}

GenericParamSymbol *
MIRSymbolIndex::resolveGenericParam(std::string_view key) const {
  auto it = genericParams.find(std::string(key));
  return it == genericParams.end() ? nullptr : it->second;
}
