#pragma once

#include "./Recover.h"

class Resolver;

class ResolverRecover final : public Recover {
public:
  ResolverRecover(Resolver &resolver);
  ~ResolverRecover() {}
  void recover() override;

private:
  Resolver &resolver;
};