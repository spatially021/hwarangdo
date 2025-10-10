#pragma once
#include "AST.h"

/*
ownership checker
*/
struct Ownership {
    void run(Program& prog);
};
