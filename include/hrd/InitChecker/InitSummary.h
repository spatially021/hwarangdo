// InitSummary.h

#pragma once

#include <unordered_map>
#include <unordered_set>

class MethodSymbol;
class TypeSymbol;
class ValueSymbol;

using InitFieldSet = std::unordered_set<ValueSymbol *>;

struct InitSummary {
  std::unordered_map<MethodSymbol *, InitFieldSet> initFields;
  std::unordered_map<TypeSymbol *, InitFieldSet> commonFields;
};