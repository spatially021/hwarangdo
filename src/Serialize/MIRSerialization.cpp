#include "hrd/Serialize/MIRSerialization.h"

#include "hrd/IR/MIR/MIRExpr.h"
#include "hrd/IR/MIR/MIRNode.h"
#include "hrd/IR/MIR/MIRStmt.h"
#include "hrd/enums/Operator.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <istream>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr std::array<char, 8> MAGIC = {'H', 'R', 'D',  'M',
                                       'I', 'R', '\0', '\1'};

enum class PlaceKind : uint8_t { Local, Param, ArrayAccess, Field, Root };
enum class ParamRefKind : uint8_t { Null, Index, Self };
enum class ValueKind : uint8_t {
  Binary,
  Load,
  PayloadExtract,
  Literal,
  ArrayInit,
  Unary,
  Cast,
  Call,
  Spawn,
  View,
  StructInit,
  Variant,
  RuntimeCall,
};
enum class StmtKind : uint8_t {
  Expr,
  Assign,
  LocalDecl,
  Quit,
  Destroy,
  Cleanup
};
enum class TermKind : uint8_t { Empty, Goto, Branch, Return, Switch };
enum class CaseKind : uint8_t { Literal, Variant };
enum class LiteralKind : uint8_t { Bool, Int, Float, Char, String };

[[noreturn]] void fail(const std::string &msg) {
  throw std::runtime_error("MIR serialization: " + msg);
}

void require(bool cond, const std::string &msg) {
  if (!cond) {
    fail(msg);
  }
}

template <typename T> void writePod(std::ostream &out, T value) {
  static_assert(std::is_trivially_copyable_v<T>);
  out.write(reinterpret_cast<const char *>(&value), sizeof(T));
  if (!out) {
    fail("failed to write stream");
  }
}

template <typename T> T readPod(std::istream &in) {
  static_assert(std::is_trivially_copyable_v<T>);
  T value{};
  in.read(reinterpret_cast<char *>(&value), sizeof(T));
  if (!in) {
    fail("unexpected end of stream");
  }
  return value;
}

void writeBool(std::ostream &out, bool value) {
  writePod<uint8_t>(out, value ? 1 : 0);
}

bool readBool(std::istream &in) {
  const auto value = readPod<uint8_t>(in);
  require(value <= 1, "invalid bool");
  return value != 0;
}

void writeString(std::ostream &out, const std::string &value) {
  require(value.size() <= std::numeric_limits<uint32_t>::max(),
          "string too large");
  writePod<uint32_t>(out, static_cast<uint32_t>(value.size()));
  out.write(value.data(), static_cast<std::streamsize>(value.size()));
  if (!out) {
    fail("failed to write string");
  }
}

std::string readString(std::istream &in) {
  const auto size = readPod<uint32_t>(in);
  std::string value(size, '\0');
  if (size != 0) {
    in.read(value.data(), static_cast<std::streamsize>(size));
    if (!in) {
      fail("unexpected end of string");
    }
  }
  return value;
}

void writeAPInt(std::ostream &out, const llvm::APInt &value) {
  writePod<uint32_t>(out, value.getBitWidth());
  const uint32_t wordCount = static_cast<uint32_t>(value.getNumWords());
  writePod<uint32_t>(out, wordCount);
  const uint64_t *words = value.getRawData();
  for (uint32_t i = 0; i < wordCount; ++i) {
    writePod<uint64_t>(out, words[i]);
  }
}

llvm::APInt readAPInt(std::istream &in) {
  const uint32_t bitWidth = readPod<uint32_t>(in);
  const uint32_t wordCount = readPod<uint32_t>(in);
  require(bitWidth != 0, "APInt bit width is zero");
  require(wordCount == llvm::APInt::getNumWords(bitWidth),
          "invalid APInt word count");

  std::vector<uint64_t> words(wordCount);
  for (uint32_t i = 0; i < wordCount; ++i) {
    words[i] = readPod<uint64_t>(in);
  }
  return llvm::APInt(bitWidth, wordCount, words.data());
}

const llvm::fltSemantics &floatSemantics(TypeSymbol *type) {
  auto *floatType = dynamic_cast<FloatType *>(type);
  require(floatType != nullptr, "float literal has non-float type");

  switch (floatType->bitWidth) {
  case 16:
    return llvm::APFloat::IEEEhalf();
  case 32:
    return llvm::APFloat::IEEEsingle();
  case 64:
    return llvm::APFloat::IEEEdouble();
  case 128:
    return llvm::APFloat::IEEEquad();
  default:
    fail("unsupported floating-point bit width");
  }
}

void writeTypeRef(std::ostream &out, TypeSymbol *type,
                  const MIRSerializationAdapter &a);
TypeSymbol *readTypeRef(std::istream &in, const MIRSerializationAdapter &a);

void writeResolvedLit(std::ostream &out, const ResolvedLit &lit,
                      const MIRSerializationAdapter &adapter) {
  writeTypeRef(out, lit.type, adapter);

  if (lit.isBool()) {
    writePod<uint8_t>(out, static_cast<uint8_t>(LiteralKind::Bool));
    writeBool(out, lit.asBool());
    return;
  }
  if (lit.isInt()) {
    writePod<uint8_t>(out, static_cast<uint8_t>(LiteralKind::Int));
    writeAPInt(out, lit.asInt().value);
    return;
  }
  if (lit.isFloat()) {
    writePod<uint8_t>(out, static_cast<uint8_t>(LiteralKind::Float));
    writeAPInt(out, lit.asFloat().value.bitcastToAPInt());
    return;
  }
  if (lit.isChar()) {
    writePod<uint8_t>(out, static_cast<uint8_t>(LiteralKind::Char));
    writePod<uint32_t>(out, lit.asChar().codePoint);
    return;
  }
  if (lit.isString()) {
    writePod<uint8_t>(out, static_cast<uint8_t>(LiteralKind::String));
    const auto &points = lit.asString().codePoints;
    require(points.size() <= std::numeric_limits<uint32_t>::max(),
            "string literal too large");
    writePod<uint32_t>(out, static_cast<uint32_t>(points.size()));
    for (uint32_t point : points) {
      writePod<uint32_t>(out, point);
    }
    return;
  }
  fail("unknown ResolvedLit kind");
}

ResolvedLit readResolvedLit(std::istream &in,
                            const MIRSerializationAdapter &adapter) {
  TypeSymbol *type = readTypeRef(in, adapter);
  const auto kind = static_cast<LiteralKind>(readPod<uint8_t>(in));

  switch (kind) {
  case LiteralKind::Bool:
    return ResolvedLit{type, readBool(in)};
  case LiteralKind::Int:
    return ResolvedLit{type, IntPayload(readAPInt(in))};
  case LiteralKind::Float: {
    llvm::APInt bits = readAPInt(in);
    return ResolvedLit{type,
                       FloatPayload(llvm::APFloat(floatSemantics(type), bits))};
  }
  case LiteralKind::Char:
    return ResolvedLit{type, CharPayload(readPod<uint32_t>(in))};
  case LiteralKind::String: {
    const uint32_t count = readPod<uint32_t>(in);
    std::vector<uint32_t> points;
    points.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
      points.push_back(readPod<uint32_t>(in));
    }
    return ResolvedLit{type, StringPayload(std::move(points))};
  }
  }
  fail("invalid ResolvedLit kind");
}

template <typename T>
void requireCallback(const std::function<T> &fn, const char *name) {
  if (!fn) {
    fail(std::string("missing adapter callback: ") + name);
  }
}

struct FunctionWriteContext {
  const MIRFunction &function;
  const MIRSerializationAdapter &adapter;
  std::unordered_map<ValueSymbol *, uint32_t> locals;
};

struct FunctionReadContext {
  MIRFunction &function;
  const MIRSerializationAdapter &adapter;
  std::vector<ValueSymbol *> locals;
};

std::string typeKey(TypeSymbol *symbol, const MIRSerializationAdapter &a) {
  require(symbol != nullptr, "null TypeSymbol");
  requireCallback(a.typeKey, "typeKey");
  return a.typeKey(symbol);
}

TypeSymbol *resolveType(const std::string &key,
                        const MIRSerializationAdapter &a) {
  requireCallback(a.resolveType, "resolveType");
  auto *result = a.resolveType(key);
  require(result != nullptr, "unresolved type: " + key);
  return result;
}

std::string methodKey(MethodSymbol *symbol, const MIRSerializationAdapter &a) {
  require(symbol != nullptr, "null MethodSymbol");
  requireCallback(a.methodKey, "methodKey");
  return a.methodKey(symbol);
}

MethodSymbol *resolveMethod(const std::string &key,
                            const MIRSerializationAdapter &a) {
  requireCallback(a.resolveMethod, "resolveMethod");
  auto *result = a.resolveMethod(key);
  require(result != nullptr, "unresolved method: " + key);
  return result;
}

void writeTypeRef(std::ostream &out, TypeSymbol *type,
                  const MIRSerializationAdapter &a) {
  writeString(out, typeKey(type, a));
}

TypeSymbol *readTypeRef(std::istream &in, const MIRSerializationAdapter &a) {
  return resolveType(readString(in), a);
}

void writeOptionalMethodRef(std::ostream &out, MethodSymbol *method,
                            const MIRSerializationAdapter &a) {
  writeBool(out, method != nullptr);
  if (method != nullptr) {
    writeString(out, methodKey(method, a));
  }
}

MethodSymbol *readOptionalMethodRef(std::istream &in,
                                    const MIRSerializationAdapter &a) {
  return readBool(in) ? resolveMethod(readString(in), a) : nullptr;
}

void collectLocals(FunctionWriteContext &ctx) {
  for (const auto &block : ctx.function.blocks) {
    for (const auto &stmt : block->stmts) {
      auto *decl = dynamic_cast<MIRLocalDeclStmt *>(stmt.get());
      if (decl == nullptr) {
        continue;
      }
      if (ctx.locals.find(decl->symbol) == ctx.locals.end()) {
        const auto id = static_cast<uint32_t>(ctx.locals.size());
        ctx.locals.emplace(decl->symbol, id);
      }
    }
  }
}

uint32_t localId(ValueSymbol *symbol, const FunctionWriteContext &ctx) {
  auto it = ctx.locals.find(symbol);
  require(it != ctx.locals.end(), "local symbol has no MIRLocalDeclStmt: " +
                                      (symbol ? symbol->name : "<null>"));
  return it->second;
}

ValueSymbol *localById(uint32_t id, const FunctionReadContext &ctx) {
  require(id < ctx.locals.size(), "invalid local id");
  return ctx.locals[id];
}

void writePlace(std::ostream &, const MIRPlace &, FunctionWriteContext &);
std::unique_ptr<MIRPlace> readPlace(std::istream &, FunctionReadContext &);
void writeValue(std::ostream &, const MIRValue &, FunctionWriteContext &);
std::unique_ptr<MIRValue> readValue(std::istream &, FunctionReadContext &);

void writeParamRef(std::ostream &out, ValueSymbol *symbol,
                   FunctionWriteContext &ctx) {
  auto *method = ctx.function.symbol;

  if (symbol == nullptr) {
    writePod<uint8_t>(out, static_cast<uint8_t>(ParamRefKind::Null));
    return;
  }

  if (method != nullptr && method->selfReceiver == symbol) {
    writePod<uint8_t>(out, static_cast<uint8_t>(ParamRefKind::Self));
    return;
  }

  require(method != nullptr, "parameter reference in default initializer");
  for (uint32_t i = 0; i < method->params.size(); ++i) {
    if (method->params[i] == symbol) {
      writePod<uint8_t>(out, static_cast<uint8_t>(ParamRefKind::Index));
      writePod<uint32_t>(out, i);
      return;
    }
  }

  fail("MIRParamPlace symbol is neither self nor a method parameter");
}

ValueSymbol *readParamRef(std::istream &in, FunctionReadContext &ctx) {
  const auto kind = static_cast<ParamRefKind>(readPod<uint8_t>(in));
  switch (kind) {
  case ParamRefKind::Null:
    return nullptr;
  case ParamRefKind::Self:
    require(ctx.function.symbol != nullptr,
            "self reference in default initializer");
    return ctx.function.symbol->selfReceiver;
  case ParamRefKind::Index: {
    require(ctx.function.symbol != nullptr,
            "parameter reference in default initializer");
    const auto index = readPod<uint32_t>(in);
    require(index < ctx.function.symbol->params.size(), "invalid param index");
    return ctx.function.symbol->params[index];
  }
  }
  fail("invalid parameter reference kind");
}

void writePlace(std::ostream &out, const MIRPlace &place,
                FunctionWriteContext &ctx) {
  if (auto *local = dynamic_cast<const MIRLocalPlace *>(&place)) {
    writePod<uint8_t>(out, static_cast<uint8_t>(PlaceKind::Local));
    writePod<uint32_t>(out, localId(local->symbol, ctx));
    return;
  }

  if (auto *param = dynamic_cast<const MIRParamPlace *>(&place)) {
    writePod<uint8_t>(out, static_cast<uint8_t>(PlaceKind::Param));
    writeParamRef(out, param->symbol, ctx);
    return;
  }

  if (auto *array = dynamic_cast<const MIRArrayAccessPlace *>(&place)) {
    writePod<uint8_t>(out, static_cast<uint8_t>(PlaceKind::ArrayAccess));
    writeTypeRef(out, array->ownType, ctx.adapter);
    writePlace(out, *array->base, ctx);
    writeValue(out, *array->index, ctx);
    return;
  }

  if (auto *field = dynamic_cast<const MIRFieldPlace *>(&place)) {
    writePod<uint8_t>(out, static_cast<uint8_t>(PlaceKind::Field));
    requireCallback(ctx.adapter.fieldKey, "fieldKey");
    writeString(out, ctx.adapter.fieldKey(field->symbol));
    writeTypeRef(out, field->ownType, ctx.adapter);
    writePlace(out, *field->base, ctx);
    return;
  }

  if (dynamic_cast<const MIRRootPlace *>(&place) != nullptr) {
    writePod<uint8_t>(out, static_cast<uint8_t>(PlaceKind::Root));
    return;
  }

  fail("unknown MIRPlace subtype");
}

std::unique_ptr<MIRPlace> readPlace(std::istream &in,
                                    FunctionReadContext &ctx) {
  const auto kind = static_cast<PlaceKind>(readPod<uint8_t>(in));
  switch (kind) {
  case PlaceKind::Local:
    return std::make_unique<MIRLocalPlace>(
        localById(readPod<uint32_t>(in), ctx));

  case PlaceKind::Param:
    return std::make_unique<MIRParamPlace>(readParamRef(in, ctx));

  case PlaceKind::ArrayAccess: {
    auto *ownType = readTypeRef(in, ctx.adapter);
    auto base = readPlace(in, ctx);
    auto index = readValue(in, ctx);
    auto *symbol = base->symbol;
    return std::make_unique<MIRArrayAccessPlace>(
        std::move(base), std::move(index), symbol, ownType);
  }

  case PlaceKind::Field: {
    const auto key = readString(in);
    requireCallback(ctx.adapter.resolveField, "resolveField");
    auto *field = ctx.adapter.resolveField(key);
    require(field != nullptr, "unresolved field: " + key);
    auto *ownType = readTypeRef(in, ctx.adapter);
    auto base = readPlace(in, ctx);
    return std::make_unique<MIRFieldPlace>(field, std::move(base), ownType);
  }

  case PlaceKind::Root:
    return std::make_unique<MIRRootPlace>();
  }
  fail("invalid MIRPlace kind");
}

void writeValueHeader(std::ostream &out, const MIRValue &value,
                      FunctionWriteContext &ctx) {
  writeTypeRef(out, value.type, ctx.adapter);
  writePod<uint8_t>(out, static_cast<uint8_t>(value.valueCategory));
}

void restoreValueHeader(MIRValue &value, MIRValueCategory category) {
  value.valueCategory = category;
}

void writeValue(std::ostream &out, const MIRValue &value,
                FunctionWriteContext &ctx) {
  writeValueHeader(out, value, ctx);

  if (auto *expr = dynamic_cast<const MIRBinaryExpr *>(&value)) {
    writePod<uint8_t>(out, static_cast<uint8_t>(ValueKind::Binary));
    writePod<uint32_t>(out, static_cast<uint32_t>(expr->op));
    writeTypeRef(out, expr->operandType, ctx.adapter);
    writeValue(out, *expr->lhs, ctx);
    writeValue(out, *expr->rhs, ctx);
    return;
  }

  if (auto *expr = dynamic_cast<const MIRLoad *>(&value)) {
    writePod<uint8_t>(out, static_cast<uint8_t>(ValueKind::Load));
    writePlace(out, *expr->place, ctx);
    return;
  }

  if (auto *expr = dynamic_cast<const MIRPayloadExtractExpr *>(&value)) {
    writePod<uint8_t>(out, static_cast<uint8_t>(ValueKind::PayloadExtract));
    requireCallback(ctx.adapter.variantKey, "variantKey");
    writeString(out, ctx.adapter.variantKey(expr->symbol));
    writeValue(out, *expr->enumValue, ctx);
    return;
  }

  if (auto *expr = dynamic_cast<const MIRLiteralExpr *>(&value)) {
    writePod<uint8_t>(out, static_cast<uint8_t>(ValueKind::Literal));
    writeResolvedLit(out, expr->literal, ctx.adapter);
    return;
  }

  if (auto *expr = dynamic_cast<const MIRArrayInitExpr *>(&value)) {
    writePod<uint8_t>(out, static_cast<uint8_t>(ValueKind::ArrayInit));
    writeTypeRef(out, expr->elementType, ctx.adapter);
    writePod<uint32_t>(out, static_cast<uint32_t>(expr->elements.size()));
    for (const auto &element : expr->elements) {
      writeValue(out, *element, ctx);
    }
    return;
  }

  if (auto *expr = dynamic_cast<const MIRUnaryExpr *>(&value)) {
    writePod<uint8_t>(out, static_cast<uint8_t>(ValueKind::Unary));
    writePod<uint32_t>(out, static_cast<uint32_t>(expr->op));
    writeValue(out, *expr->operrand, ctx);
    return;
  }

  if (auto *expr = dynamic_cast<const MIRCastExpr *>(&value)) {
    writePod<uint8_t>(out, static_cast<uint8_t>(ValueKind::Cast));
    writeTypeRef(out, expr->from, ctx.adapter);
    writeTypeRef(out, expr->to, ctx.adapter);
    writeValue(out, *expr->operrand, ctx);
    return;
  }

  if (auto *expr = dynamic_cast<const MIRCallExpr *>(&value)) {
    writePod<uint8_t>(out, static_cast<uint8_t>(ValueKind::Call));
    writeBool(out, expr->base != nullptr);
    if (expr->base) {
      writeValue(out, *expr->base, ctx);
    }
    writeString(out, methodKey(expr->method, ctx.adapter));

    writePod<uint32_t>(out, static_cast<uint32_t>(expr->args.size()));
    for (const auto &arg : expr->args) {
      writeValue(out, *arg, ctx);
    }

    writePod<uint32_t>(out, static_cast<uint32_t>(expr->genericArgs.size()));
    for (auto *arg : expr->genericArgs) {
      writeTypeRef(out, arg, ctx.adapter);
    }

    requireCallback(ctx.adapter.genericParamKey, "genericParamKey");
    std::vector<std::pair<std::string, TypeSymbol *>> substitutions;
    substitutions.reserve(expr->substitution.size());
    for (const auto &[param, type] : expr->substitution) {
      substitutions.emplace_back(ctx.adapter.genericParamKey(param), type);
    }
    std::sort(substitutions.begin(), substitutions.end(),
              [](const auto &a, const auto &b) { return a.first < b.first; });

    writePod<uint32_t>(out, static_cast<uint32_t>(substitutions.size()));
    for (const auto &[key, type] : substitutions) {
      writeString(out, key);
      writeTypeRef(out, type, ctx.adapter);
    }
    return;
  }

  if (auto *expr = dynamic_cast<const MIRSpawnExpr *>(&value)) {
    writePod<uint8_t>(out, static_cast<uint8_t>(ValueKind::Spawn));
    writeTypeRef(out, expr->entityType, ctx.adapter);
    writeOptionalMethodRef(out, expr->initMethod, ctx.adapter);
    writePod<uint32_t>(out, static_cast<uint32_t>(expr->args.size()));
    for (const auto &arg : expr->args) {
      writeValue(out, *arg, ctx);
    }
    return;
  }

  if (auto *expr = dynamic_cast<const MIRViewExpr *>(&value)) {
    writePod<uint8_t>(out, static_cast<uint8_t>(ValueKind::View));
    writeTypeRef(out, expr->entityType, ctx.adapter);
    writeValue(out, *expr->handle, ctx);
    return;
  }

  if (auto *expr = dynamic_cast<const MIRStructInitExpr *>(&value)) {
    writePod<uint8_t>(out, static_cast<uint8_t>(ValueKind::StructInit));
    writeTypeRef(out, expr->structType, ctx.adapter);
    writeOptionalMethodRef(out, expr->initMethod, ctx.adapter);
    writePod<uint32_t>(out, static_cast<uint32_t>(expr->args.size()));
    for (const auto &arg : expr->args) {
      writeValue(out, *arg, ctx);
    }
    writePod<uint32_t>(out, static_cast<uint32_t>(expr->genericArgs.size()));
    for (auto *arg : expr->genericArgs) {
      writeTypeRef(out, arg, ctx.adapter);
    }
    return;
  }

  if (auto *expr = dynamic_cast<const MIRVariantExpr *>(&value)) {
    writePod<uint8_t>(out, static_cast<uint8_t>(ValueKind::Variant));
    requireCallback(ctx.adapter.variantKey, "variantKey");
    writeString(out, ctx.adapter.variantKey(expr->variant));
    writeBool(out, expr->payload != nullptr);
    if (expr->payload) {
      writeValue(out, *expr->payload, ctx);
    }
    return;
  }

  if (auto *expr = dynamic_cast<const MIRRuntimeCallExpr *>(&value)) {
    writePod<uint8_t>(out, static_cast<uint8_t>(ValueKind::RuntimeCall));
    requireCallback(ctx.adapter.runtimeKey, "runtimeKey");
    writeString(out, ctx.adapter.runtimeKey(expr->symbol));
    writePod<uint32_t>(out, static_cast<uint32_t>(expr->args.size()));
    for (const auto &arg : expr->args) {
      writeValue(out, *arg, ctx);
    }
    return;
  }

  fail("unknown MIRValue subtype");
}

std::unique_ptr<MIRValue> readValue(std::istream &in,
                                    FunctionReadContext &ctx) {
  auto *resultType = readTypeRef(in, ctx.adapter);
  const auto category = static_cast<MIRValueCategory>(readPod<uint8_t>(in));
  const auto kind = static_cast<ValueKind>(readPod<uint8_t>(in));

  std::unique_ptr<MIRValue> result;

  switch (kind) {
  case ValueKind::Binary: {
    const auto op = static_cast<Operator>(readPod<uint32_t>(in));
    auto *operandType = readTypeRef(in, ctx.adapter);
    auto lhs = readValue(in, ctx);
    auto rhs = readValue(in, ctx);
    result = std::make_unique<MIRBinaryExpr>(std::move(lhs), std::move(rhs), op,
                                             resultType, operandType);
    break;
  }
  case ValueKind::Load:
    result = std::make_unique<MIRLoad>(readPlace(in, ctx), resultType);
    break;

  case ValueKind::PayloadExtract: {
    const auto key = readString(in);
    requireCallback(ctx.adapter.resolveVariant, "resolveVariant");
    auto *variant = ctx.adapter.resolveVariant(key);
    require(variant != nullptr, "unresolved enum variant: " + key);
    result = std::make_unique<MIRPayloadExtractExpr>(
        variant, readValue(in, ctx), resultType);
    break;
  }

  case ValueKind::Literal: {
    result = std::make_unique<MIRLiteralExpr>(readResolvedLit(in, ctx.adapter),
                                              resultType);
    break;
  }

  case ValueKind::ArrayInit: {
    auto *elementType = readTypeRef(in, ctx.adapter);
    const auto count = readPod<uint32_t>(in);
    std::vector<std::unique_ptr<MIRValue>> elements;
    elements.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
      elements.push_back(readValue(in, ctx));
    }
    result = std::make_unique<MIRArrayInitExpr>(resultType, elementType,
                                                std::move(elements));
    break;
  }

  case ValueKind::Unary: {
    const auto op = static_cast<Operator>(readPod<uint32_t>(in));
    result = std::make_unique<MIRUnaryExpr>(readValue(in, ctx), op, resultType);
    break;
  }

  case ValueKind::Cast: {
    auto *from = readTypeRef(in, ctx.adapter);
    auto *to = readTypeRef(in, ctx.adapter);
    result = std::make_unique<MIRCastExpr>(readValue(in, ctx), from, to);
    break;
  }

  case ValueKind::Call: {
    std::unique_ptr<MIRValue> base;
    if (readBool(in)) {
      base = readValue(in, ctx);
    }
    auto *method = resolveMethod(readString(in), ctx.adapter);

    const auto argCount = readPod<uint32_t>(in);
    std::vector<std::unique_ptr<MIRValue>> args;
    args.reserve(argCount);
    for (uint32_t i = 0; i < argCount; ++i) {
      args.push_back(readValue(in, ctx));
    }

    const auto genericCount = readPod<uint32_t>(in);
    std::vector<TypeSymbol *> genericArgs;
    genericArgs.reserve(genericCount);
    for (uint32_t i = 0; i < genericCount; ++i) {
      genericArgs.push_back(readTypeRef(in, ctx.adapter));
    }

    const auto substitutionCount = readPod<uint32_t>(in);
    std::unordered_map<GenericParamSymbol *, TypeSymbol *> substitution;
    for (uint32_t i = 0; i < substitutionCount; ++i) {
      const auto key = readString(in);
      requireCallback(ctx.adapter.resolveGenericParam, "resolveGenericParam");
      auto *param = ctx.adapter.resolveGenericParam(key);
      require(param != nullptr, "unresolved generic parameter: " + key);
      substitution.emplace(param, readTypeRef(in, ctx.adapter));
    }

    result = std::make_unique<MIRCallExpr>(std::move(base), std::move(args),
                                           std::move(genericArgs), method,
                                           resultType, std::move(substitution));
    break;
  }

  case ValueKind::Spawn: {
    auto *entityType = readTypeRef(in, ctx.adapter);
    auto *init = readOptionalMethodRef(in, ctx.adapter);
    const auto count = readPod<uint32_t>(in);
    std::vector<std::unique_ptr<MIRValue>> args;
    args.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
      args.push_back(readValue(in, ctx));
    }
    result = std::make_unique<MIRSpawnExpr>(entityType, init, std::move(args),
                                            resultType);
    break;
  }

  case ValueKind::View: {
    auto *entityType = readTypeRef(in, ctx.adapter);
    result = std::make_unique<MIRViewExpr>(entityType, readValue(in, ctx),
                                           resultType);
    break;
  }

  case ValueKind::StructInit: {
    auto *structType = readTypeRef(in, ctx.adapter);
    auto *init = readOptionalMethodRef(in, ctx.adapter);
    const auto argCount = readPod<uint32_t>(in);
    std::vector<std::unique_ptr<MIRValue>> args;
    args.reserve(argCount);
    for (uint32_t i = 0; i < argCount; ++i) {
      args.push_back(readValue(in, ctx));
    }
    const auto genericCount = readPod<uint32_t>(in);
    std::vector<TypeSymbol *> genericArgs;
    genericArgs.reserve(genericCount);
    for (uint32_t i = 0; i < genericCount; ++i) {
      genericArgs.push_back(readTypeRef(in, ctx.adapter));
    }
    result = std::make_unique<MIRStructInitExpr>(
        structType, init, std::move(args), std::move(genericArgs), resultType);
    break;
  }

  case ValueKind::Variant: {
    const auto key = readString(in);
    requireCallback(ctx.adapter.resolveVariant, "resolveVariant");
    auto *variant = ctx.adapter.resolveVariant(key);
    require(variant != nullptr, "unresolved enum variant: " + key);
    std::unique_ptr<MIRValue> payload;
    if (readBool(in)) {
      payload = readValue(in, ctx);
    }
    result = std::make_unique<MIRVariantExpr>(variant, std::move(payload),
                                              resultType);
    break;
  }

  case ValueKind::RuntimeCall: {
    const auto key = readString(in);
    requireCallback(ctx.adapter.resolveRuntime, "resolveRuntime");
    auto *runtime = ctx.adapter.resolveRuntime(key);
    require(runtime != nullptr, "unresolved runtime symbol: " + key);
    const auto count = readPod<uint32_t>(in);
    std::vector<std::unique_ptr<MIRValue>> args;
    args.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
      args.push_back(readValue(in, ctx));
    }
    result = std::make_unique<MIRRuntimeCallExpr>(runtime, std::move(args),
                                                  resultType);
    break;
  }
  }

  require(result != nullptr, "invalid MIRValue kind");
  restoreValueHeader(*result, category);
  return result;
}

void writeStmt(std::ostream &out, const MIRStmt &stmt,
               FunctionWriteContext &ctx) {
  if (auto *s = dynamic_cast<const MIRExprStmt *>(&stmt)) {
    writePod<uint8_t>(out, static_cast<uint8_t>(StmtKind::Expr));
    writeValue(out, *s->expr, ctx);
    return;
  }
  if (auto *s = dynamic_cast<const MIRAssignStmt *>(&stmt)) {
    writePod<uint8_t>(out, static_cast<uint8_t>(StmtKind::Assign));
    writePlace(out, *s->lhs, ctx);
    writeValue(out, *s->rhs, ctx);
    return;
  }
  if (auto *s = dynamic_cast<const MIRLocalDeclStmt *>(&stmt)) {
    writePod<uint8_t>(out, static_cast<uint8_t>(StmtKind::LocalDecl));
    writePod<uint32_t>(out, localId(s->symbol, ctx));
    writeTypeRef(out, s->type, ctx.adapter);
    writeBool(out, s->init != nullptr);
    if (s->init) {
      writeValue(out, *s->init, ctx);
    }
    return;
  }
  if (dynamic_cast<const MIRQuitStmt *>(&stmt) != nullptr) {
    writePod<uint8_t>(out, static_cast<uint8_t>(StmtKind::Quit));
    return;
  }
  if (auto *s = dynamic_cast<const MIRDestroyStmt *>(&stmt)) {
    writePod<uint8_t>(out, static_cast<uint8_t>(StmtKind::Destroy));
    writeTypeRef(out, s->type, ctx.adapter);
    writeValue(out, *s->handlePlace, ctx);
    return;
  }
  if (auto *s = dynamic_cast<const MIRCleanupStmt *>(&stmt)) {
    writePod<uint8_t>(out, static_cast<uint8_t>(StmtKind::Cleanup));
    writePod<uint32_t>(out, static_cast<uint32_t>(s->locals.size()));
    for (auto *local : s->locals) {
      writePod<uint32_t>(out, localId(local, ctx));
    }
    return;
  }
  fail("unknown MIRStmt subtype");
}

std::unique_ptr<MIRStmt> readStmt(std::istream &in, FunctionReadContext &ctx) {
  const auto kind = static_cast<StmtKind>(readPod<uint8_t>(in));
  switch (kind) {
  case StmtKind::Expr:
    return std::make_unique<MIRExprStmt>(readValue(in, ctx));

  case StmtKind::Assign: {
    auto lhs = readPlace(in, ctx);
    auto rhs = readValue(in, ctx);
    return std::make_unique<MIRAssignStmt>(std::move(lhs), std::move(rhs));
  }

  case StmtKind::LocalDecl: {
    auto *symbol = localById(readPod<uint32_t>(in), ctx);
    auto *type = readTypeRef(in, ctx.adapter);
    std::unique_ptr<MIRValue> init;
    if (readBool(in)) {
      init = readValue(in, ctx);
    }
    return std::make_unique<MIRLocalDeclStmt>(type, symbol, std::move(init));
  }

  case StmtKind::Quit:
    return std::make_unique<MIRQuitStmt>();

  case StmtKind::Destroy: {
    auto *type = readTypeRef(in, ctx.adapter);
    return std::make_unique<MIRDestroyStmt>(readValue(in, ctx), type);
  }

  case StmtKind::Cleanup: {
    const auto count = readPod<uint32_t>(in);
    std::vector<ValueSymbol *> locals;
    locals.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
      locals.push_back(localById(readPod<uint32_t>(in), ctx));
    }
    return std::make_unique<MIRCleanupStmt>(std::move(locals));
  }
  }
  fail("invalid MIRStmt kind");
}

void writeTerminator(std::ostream &out, const MIRTerminator &term,
                     FunctionWriteContext &ctx) {
  if (std::holds_alternative<std::monostate>(term)) {
    writePod<uint8_t>(out, static_cast<uint8_t>(TermKind::Empty));
    return;
  }
  if (auto *t = std::get_if<GotoTerminator>(&term)) {
    writePod<uint8_t>(out, static_cast<uint8_t>(TermKind::Goto));
    writePod<BlockID>(out, t->targetBlock);
    return;
  }
  if (auto *t = std::get_if<BranchTerminator>(&term)) {
    writePod<uint8_t>(out, static_cast<uint8_t>(TermKind::Branch));
    writeValue(out, *t->cond, ctx);
    writePod<BlockID>(out, t->trueBlock);
    writePod<BlockID>(out, t->falseBlock);
    return;
  }
  if (auto *t = std::get_if<ReturnTerminator>(&term)) {
    writePod<uint8_t>(out, static_cast<uint8_t>(TermKind::Return));
    writeBool(out, t->value != nullptr);
    if (t->value) {
      writeValue(out, *t->value, ctx);
    }
    return;
  }
  if (auto *t = std::get_if<SwitchTerminator>(&term)) {
    writePod<uint8_t>(out, static_cast<uint8_t>(TermKind::Switch));
    writeValue(out, *t->cond, ctx);
    writePod<uint32_t>(out, static_cast<uint32_t>(t->cases.size()));
    for (const auto &c : t->cases) {
      if (auto *lit = std::get_if<ResolvedLit>(&c.value)) {
        writePod<uint8_t>(out, static_cast<uint8_t>(CaseKind::Literal));
        writeResolvedLit(out, *lit, ctx.adapter);
      } else if (auto *variant = std::get_if<EnumVariantSymbol *>(&c.value)) {
        writePod<uint8_t>(out, static_cast<uint8_t>(CaseKind::Variant));
        requireCallback(ctx.adapter.variantKey, "variantKey");
        writeString(out, ctx.adapter.variantKey(*variant));
      } else {
        fail("invalid switch case value");
      }
      writePod<BlockID>(out, c.target);
    }
    writePod<BlockID>(out, t->defaultTarget);
    return;
  }
  fail("unknown MIR terminator");
}

MIRTerminator readTerminator(std::istream &in, FunctionReadContext &ctx) {
  const auto kind = static_cast<TermKind>(readPod<uint8_t>(in));
  switch (kind) {
  case TermKind::Empty:
    return std::monostate{};
  case TermKind::Goto:
    return GotoTerminator(readPod<BlockID>(in));
  case TermKind::Branch: {
    auto cond = readValue(in, ctx);
    const auto trueBlock = readPod<BlockID>(in);
    const auto falseBlock = readPod<BlockID>(in);
    return BranchTerminator(std::move(cond), trueBlock, falseBlock);
  }
  case TermKind::Return: {
    std::unique_ptr<MIRValue> value;
    if (readBool(in)) {
      value = readValue(in, ctx);
    }
    return ReturnTerminator(std::move(value));
  }
  case TermKind::Switch: {
    auto cond = readValue(in, ctx);
    const auto count = readPod<uint32_t>(in);
    std::vector<MIRCase> cases;
    cases.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
      const auto caseKind = static_cast<CaseKind>(readPod<uint8_t>(in));
      if (caseKind == CaseKind::Literal) {
        auto literal = readResolvedLit(in, ctx.adapter);
        const auto target = readPod<BlockID>(in);
        cases.emplace_back(std::move(literal), target);
      } else if (caseKind == CaseKind::Variant) {
        const auto key = readString(in);
        requireCallback(ctx.adapter.resolveVariant, "resolveVariant");
        auto *variant = ctx.adapter.resolveVariant(key);
        require(variant != nullptr, "unresolved enum variant: " + key);
        const auto target = readPod<BlockID>(in);
        cases.emplace_back(variant, target);
      } else {
        fail("invalid switch case kind");
      }
    }
    const auto defaultTarget = readPod<BlockID>(in);
    return SwitchTerminator(std::move(cond), std::move(cases), defaultTarget);
  }
  }
  fail("invalid MIR terminator kind");
}

void writeFunction(std::ostream &out, const MIRFunction &function,
                   const MIRSerializationAdapter &adapter) {
  FunctionWriteContext ctx{function, adapter, {}};
  collectLocals(ctx);

  writeBool(out, function.isDefaultInit);
  writeTypeRef(out, function.owner, adapter);
  if (!function.isDefaultInit) {
    writeString(out, methodKey(function.symbol, adapter));
  }

  writePod<BlockID>(out, function.entry);
  writePod<uint32_t>(out, function.nextTemp);

  // Local table is emitted before blocks, allowing references from any block.
  std::vector<std::pair<uint32_t, ValueSymbol *>> locals;
  locals.reserve(ctx.locals.size());
  for (const auto &[symbol, id] : ctx.locals) {
    locals.emplace_back(id, symbol);
  }
  std::sort(locals.begin(), locals.end());

  writePod<uint32_t>(out, static_cast<uint32_t>(locals.size()));
  for (const auto &[id, symbol] : locals) {
    (void)id;
    writeString(out, symbol->name);
    writeTypeRef(out, symbol->typeSymbol, adapter);
  }

  writePod<uint32_t>(out, static_cast<uint32_t>(function.blocks.size()));
  for (const auto &block : function.blocks) {
    writePod<BlockID>(out, block->id);
    writePod<uint32_t>(out, static_cast<uint32_t>(block->stmts.size()));
    for (const auto &stmt : block->stmts) {
      writeStmt(out, *stmt, ctx);
    }
    writeTerminator(out, block->terminator, ctx);
  }
}

std::unique_ptr<MIRFunction>
readFunction(std::istream &in, const MIRSerializationAdapter &adapter) {
  const bool isDefaultInit = readBool(in);
  auto *owner = readTypeRef(in, adapter);
  MethodSymbol *method = nullptr;
  if (!isDefaultInit) {
    method = resolveMethod(readString(in), adapter);
  }

  auto function = std::make_unique<MIRFunction>(method, owner);
  function->isDefaultInit = isDefaultInit;
  function->entry = readPod<BlockID>(in);
  function->nextTemp = readPod<uint32_t>(in);

  FunctionReadContext ctx{*function, adapter, {}};

  const auto localCount = readPod<uint32_t>(in);
  ctx.locals.reserve(localCount);
  requireCallback(adapter.ownLocal, "ownLocal");
  for (uint32_t i = 0; i < localCount; ++i) {
    auto symbol = std::make_unique<ValueSymbol>();
    symbol->name = readString(in);
    symbol->typeSymbol = readTypeRef(in, adapter);
    auto *raw = adapter.ownLocal(std::move(symbol));
    require(raw != nullptr, "ownLocal returned nullptr");
    ctx.locals.push_back(raw);
  }

  const auto blockCount = readPod<uint32_t>(in);
  function->blocks.reserve(blockCount);
  for (uint32_t i = 0; i < blockCount; ++i) {
    const auto id = readPod<BlockID>(in);
    require(id == i,
            "non-dense block ids are not supported by MIRFunction::getBlock");
    auto block = std::make_unique<BasicBlock>(id);
    const auto stmtCount = readPod<uint32_t>(in);
    block->stmts.reserve(stmtCount);
    for (uint32_t s = 0; s < stmtCount; ++s) {
      block->stmts.push_back(readStmt(in, ctx));
    }
    block->terminator = readTerminator(in, ctx);
    function->blocks.push_back(std::move(block));
  }

  require(function->blocks.empty() || function->entry < function->blocks.size(),
          "invalid function entry block");
  return function;
}

void rebuildGenericIndex(MIRProgram &program, MIRFunction *function) {
  auto *owner = function->owner;
  require(owner != nullptr, "generic MIR function without owner");

  auto &decl = program.genericMap[owner];
  decl.symbol = owner;
  if (function->isDefaultInit) {
    decl.defaultInit = function;
    return;
  }

  require(function->symbol != nullptr, "generic method without MethodSymbol");
  decl.methods.push_back(function);
  decl.methodMap.emplace(function->symbol, function);
}

} // namespace

void MIRSerializer::write(std::ostream &out, const MIRProgram &program,
                          const MIRSerializationAdapter &adapter) {
  out.write(MAGIC.data(), static_cast<std::streamsize>(MAGIC.size()));
  if (!out) {
    fail("failed to write magic");
  }
  writePod<uint32_t>(out, FORMAT_VERSION);

  writePod<uint32_t>(out, static_cast<uint32_t>(program.genericOrigin.size()));
  for (const auto &function : program.genericOrigin) {
    writeFunction(out, *function, adapter);
  }
}

void MIRSerializer::writeFile(const std::string &path,
                              const MIRProgram &program,
                              const MIRSerializationAdapter &adapter) {
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    fail("cannot open output file: " + path);
  }
  write(out, program, adapter);
}

std::unique_ptr<MIRProgram>
MIRDeserializer::read(std::istream &in,
                      const MIRSerializationAdapter &adapter) {
  std::array<char, MAGIC.size()> magic{};
  in.read(magic.data(), static_cast<std::streamsize>(magic.size()));
  require(in.good(), "invalid/truncated MIR file");
  require(magic == MAGIC, "invalid MIR magic");

  const auto version = readPod<uint32_t>(in);
  require(version == MIRSerializer::FORMAT_VERSION,
          "unsupported MIR format version: " + std::to_string(version));

  auto program = std::make_unique<MIRProgram>();

  const auto genericCount = readPod<uint32_t>(in);
  program->genericOrigin.reserve(genericCount);
  for (uint32_t i = 0; i < genericCount; ++i) {
    auto function = readFunction(in, adapter);
    auto *raw = function.get();
    program->genericOrigin.push_back(std::move(function));

    rebuildGenericIndex(*program, raw);
  }

  return program;
}

std::unique_ptr<MIRProgram>
MIRDeserializer::readFile(const std::string &path,
                          const MIRSerializationAdapter &adapter) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    fail("cannot open input file: " + path);
  }
  return read(in, adapter);
}
