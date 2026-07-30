#include "hrd/Recover/LinkerRecover.h"
#include "hrd/SemanticAnalyzer/Linker.h"

LinkerRecover::LinkerRecover(Linker &l) : linker(l) {}

void LinkerRecover::recover() { fail(); }