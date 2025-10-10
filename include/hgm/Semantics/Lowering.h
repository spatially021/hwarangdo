#pragma once
#include "AST.h"

/*
AST lowering to simpler IR
*/
struct Lowering {
    void run(Program& prog);
};
