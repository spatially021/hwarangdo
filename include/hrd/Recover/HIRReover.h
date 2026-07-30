#pragma once

#include "./Recover.h"

class HIRBuilder;

class HIRRecover final : public Recover {
public:
  HIRRecover(HIRBuilder &builder);
  void recover();

private:
  HIRBuilder &builder;
};