#pragma once

#include "Symbol.h"

class Scope;

class StorageSymbol : public Symbol {
public:
  enum class Kind {
    WORLD,
  };

  Kind storageKind;
  Scope *memberScope = nullptr;

protected:
  void _anchor() override {};
};