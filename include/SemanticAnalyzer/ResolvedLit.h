#pragma once

#include <cstdint>
#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/APInt.h>
#include <variant>
#include <vector>

class TypeSymbol;

struct IntPayload {
  llvm::APInt value;
  IntPayload(llvm::APInt v) : value(v) {}
};
struct FloatPayload {
  llvm::APFloat value;
  FloatPayload(llvm::APFloat v) : value(v) {}
};
struct CharPayload {
  uint32_t codePoint;
  CharPayload(uint32_t v) : codePoint(v) {}
};
struct StringPayload {
  std::vector<uint32_t> codePoints;
  StringPayload(std::vector<uint32_t> c) : codePoints(c) {}
};

struct ResolvedLit {
  TypeSymbol *type;
  std::variant<bool, IntPayload, FloatPayload, CharPayload, StringPayload>
      value;

  bool isBool() const { return std::holds_alternative<bool>(value); }
  bool isInt() const { return std::holds_alternative<IntPayload>(value); }
  bool isFloat() const { return std::holds_alternative<FloatPayload>(value); }
  bool isChar() const { return std::holds_alternative<CharPayload>(value); }
  bool isString() const { return std::holds_alternative<StringPayload>(value); }

  const bool &asBool() const { return std::get<bool>(value); }
  const IntPayload &asInt() const { return std::get<IntPayload>(value); }
  const FloatPayload &asFloat() const { return std::get<FloatPayload>(value); }
  const CharPayload &asChar() const { return std::get<CharPayload>(value); }
  const StringPayload &asString() const {
    return std::get<StringPayload>(value);
  }
};