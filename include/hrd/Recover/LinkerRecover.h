#pragma once

#include "./Recover.h"

class Linker;

class LinkerRecover final : public Recover {
public:
  LinkerRecover(Linker &linker);
  ~LinkerRecover() {}
  void recover() override;

private:
  Linker &linker;
};