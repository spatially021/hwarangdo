#include "hgm/Driver.h"
#include "hgm/Lexer.h"
#include "hgm/Parser.h"
#include "hgm/Semantics/TypeChecker.h"
#include "hgm/Semantics/Ownership.h"
#include "hgm/Semantics/Lowering.h"
#include "hgm/Backend/Codegen.h"
#include "hgm/Backend/Builtins.h"
#include <fstream>
#include <sstream>
#include <iostream>

int Driver::compileFile(const std::string& inPath,const std::string& outLlPath){
    std::ifstream ifs(inPath);
    std::stringstream buf;
    buf << ifs.rdbuf();

    Lexer lex(buf.str());
    auto toks = lex.tokenize();

    Parser parser(toks);
    Program prog = parser.parseProgram();

    TypeChecker().run(prog);
    Ownership().run(prog);
    Lowering().run(prog);

    Builtins::registerBuiltins();

    Codegen cg;
    cg.emit(prog);
    cg.writeIRToFile(outLlPath);
    return 0;
}
