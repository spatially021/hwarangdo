#pragma once

#include "vector"

class MethodSymbol;

enum FailKind {
  None,
  NotFound,
  InstanceMethodAsStatic,
  StaticMethodAsInstance,
};

struct MethodBucket {
  std::vector<MethodSymbol *> *bucket;
  FailKind kind;
};