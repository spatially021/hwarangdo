#pragma once
#include "AST.h"
#include <string>

/*
LLVM IR code generator
*/
struct Codegen {
    Codegen();
    void emit(const Program& prog);
    void writeIRToFile(const std::string& path);
};
