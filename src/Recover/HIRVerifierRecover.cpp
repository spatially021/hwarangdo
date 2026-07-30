#include "hrd/Recover/HIRVerifierRecover.h"

HIRVerifierRecover::HIRVerifierRecover(HIRVerifier &v) : verifier(v) {}
void HIRVerifierRecover::recover() { fail(); }