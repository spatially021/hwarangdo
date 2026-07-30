#pragma once

#include "./Recover.h"

class Builder;

class BuilderRecover final : public Recover {
public:
  BuilderRecover(Builder &buolder);
  void recover();

private:
  Builder &builder;
};