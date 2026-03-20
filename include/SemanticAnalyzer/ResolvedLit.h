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
};