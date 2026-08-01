#include "hrd/Recover/VerifierRecover.h"
#include "hrd/SemanticAnalyzer/Verifier.h"

VerifierRecover::VerifierRecover(Verifier &v) : verifier(v) {}
void VerifierRecover::recover() { fail(); }