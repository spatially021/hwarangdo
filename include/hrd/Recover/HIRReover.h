#pragma once

#include "./Recover.h"

class HIRBuilder;

class HIRRecover final : public Recover {
public:
  HIRRecover(HIRBuilder &builder);
  ~HIRRecover() {}
  void recover() override;

private:
  HIRBuilder &builder;
};