#pragma once

#include "./Recover.h"

class Resolver;

class ResolverRecover final : public Recover {
public:
  ResolverRecover(Resolver &resolver);
  void recover();

private:
  Resolver &resolver;
};