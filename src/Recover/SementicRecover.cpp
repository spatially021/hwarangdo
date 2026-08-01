#include "hrd/Recover/SementicRecover.h"
#include "hrd/SemanticAnalyzer.h"
SemanticAnalyzerRecover::SemanticAnalyzerRecover(SemanticAnalyzer &a)
    : analyzer(a) {}
void SemanticAnalyzerRecover::recover() { fail(); }