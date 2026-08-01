#pragma once

#include "./Recover.h"

class Verifier;

class VerifierRecover final : public Recover {
public:
  VerifierRecover(Verifier &verifier);
  ~VerifierRecover() {}
  void recover();

private:
  Verifier &verifier;
};