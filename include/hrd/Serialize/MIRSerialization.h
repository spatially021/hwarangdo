#pragma once

#include "hrd/IR/MIR/MIRProgram.h"
#include "hrd/SemanticAnalyzer/ResolvedLit.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/RuntimeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"

#include <functional>
#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>

struct MIRSerializationAdapter {
  // Stable external symbol keys. These must be stable across compiler runs.
  std::function<std::string(TypeSymbol *)> typeKey;
  std::function<TypeSymbol *(std::string_view)> resolveType;

  std::function<std::string(MethodSymbol *)> methodKey;
  std::function<MethodSymbol *(std::string_view)> resolveMethod;

  std::function<std::string(ValueSymbol *)> fieldKey;
  std::function<ValueSymbol *(std::string_view)> resolveField;

  std::function<std::string(EnumVariantSymbol *)> variantKey;
  std::function<EnumVariantSymbol *(std::string_view)> resolveVariant;

  std::function<std::string(RuntimeSymbol *)> runtimeKey;
  std::function<RuntimeSymbol *(std::string_view)> resolveRuntime;

  std::function<std::string(GenericParamSymbol *)> genericParamKey;
  std::function<GenericParamSymbol *(std::string_view)> resolveGenericParam;

  // Imported MIR locals/temps need an owner whose lifetime is >= MIRProgram.
  // A natural implementation is table.registry.addTemp(std::move(symbol)).
  std::function<ValueSymbol *(std::unique_ptr<ValueSymbol>)> ownLocal;
};

class MIRSerializer {
public:
  static constexpr uint32_t FORMAT_VERSION = 2;

  static void write(std::ostream &out, const MIRProgram &program,
                    const MIRSerializationAdapter &adapter);
  static void writeFile(const std::string &path, const MIRProgram &program,
                        const MIRSerializationAdapter &adapter);
};

class MIRDeserializer {
public:
  static std::unique_ptr<MIRProgram>
  read(std::istream &in, const MIRSerializationAdapter &adapter);

  static std::unique_ptr<MIRProgram>
  readFile(const std::string &path, const MIRSerializationAdapter &adapter);
};
