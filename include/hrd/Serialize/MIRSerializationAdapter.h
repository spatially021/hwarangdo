#pragma once

#include "hrd/Serialize/MIRSerialization.h"

#include <memory>

class SymbolTable;
class MIRSymbolIndex;

struct MIRSerializationAdapterBundle {
  std::shared_ptr<MIRSymbolIndex> index;
  MIRSerializationAdapter adapter;
};

MIRSerializationAdapterBundle makeMIRSerializationAdapter(SymbolTable &table);
