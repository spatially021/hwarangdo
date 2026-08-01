#pragma once

#include "./Recover.h"

class Builder;

class BuilderRecover final : public Recover {
public:
  BuilderRecover(Builder &buolder);
  ~BuilderRecover() {}
  void recover() override;

private:
  Builder &builder;
};