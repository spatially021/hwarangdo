#pragma once

#include "./Recover.h"

class InitChecker;

class InitCheckerRecover final : public Recover {
public:
  InitCheckerRecover(InitChecker &checker);
  ~InitCheckerRecover() {}
  void recover();

private:
  InitChecker &checker;
};