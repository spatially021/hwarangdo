#include "hgm/Backend/Codegen.h"
#include <iostream>

Codegen::Codegen() {}

void Codegen::emit(const Program& prog) {
    // TODO: generate LLVM IR
    std::cout << "[Codegen] emit IR\n";
}

void Codegen::writeIRToFile(const std::string& path) {
    // TODO: write to file
    std::cout << "[Codegen] write IR to " << path << "\n";
}
