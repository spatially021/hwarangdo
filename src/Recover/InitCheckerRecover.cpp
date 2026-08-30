
#include "hrd/Recover/InitCheckerRecover.h"
#include "hrd/InitChecker/InitChecker.h"
InitCheckerRecover::InitCheckerRecover(InitChecker &init) : checker(init) {}
void InitCheckerRecover::recover() { fail(); }