#include "hrd/Recover/ParserRecover.h"

ParserRecover::ParserRecover(Parser &p) : parser(p) {}

void ParserRecover::recover() { fail(); }