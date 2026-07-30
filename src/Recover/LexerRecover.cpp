#include "hrd/Recover/LexerRecover.h"

LexerRecover::LexerRecover(Lexer &l) : lexer(l) {}
void LexerRecover::recover() { fail(); }