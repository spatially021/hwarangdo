#pragma once

#include "./Recover.h"

class HIRVerifier;

class HIRVerifierRecover final : public Recover {
public:
  HIRVerifierRecover(HIRVerifier &verifier);
  ~HIRVerifierRecover() {}
  void recover() override;

private:
  HIRVerifier &verifier;
};