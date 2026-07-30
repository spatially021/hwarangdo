
#include "hrd/Recover/ResolverRecover.h"
#include "hrd/SemanticAnalyzer/Resolver.h"
ResolverRecover::ResolverRecover(Resolver &r) : resolver(r) {}

void ResolverRecover::recover() { fail(); }