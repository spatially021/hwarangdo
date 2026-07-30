#pragma once

#include "./Recover.h"

class HIRVerifier;

class HIRVerifierRecover final : public Recover {
public:
  HIRVerifierRecover(HIRVerifier &verifier);
  void recover();

private:
  HIRVerifier &verifier;
};