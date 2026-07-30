#pragma once

#include "./Recover.h"

class Linker;

class LinkerRecover final : public Recover {
public:
  LinkerRecover(Linker &linker);
  void recover();

private:
  Linker &linker;
};