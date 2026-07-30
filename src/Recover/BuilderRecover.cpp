#include "hrd/Recover/BuilderRecover.h"
#include "hrd/SemanticAnalyzer/Builder.h"

BuilderRecover::BuilderRecover(Builder &b) : builder(b) {}

void BuilderRecover::recover() { fail(); }