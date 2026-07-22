#include "hrd/compiler/CompilerDriver.h"
#include "hrd/AST/Program.h"
#include "hrd/Color.h"
#include "hrd/Debugger/BuilderDebugger.h"
#include "hrd/Debugger/HIRDebugger.h"
#include "hrd/Debugger/MIRDebugger/MIRDebuuger.h"
#include "hrd/Debugger/MIRDebugger/MIRGraphvizDebugger.h"
#include "hrd/Debugger/ParserDebugger.h"
#include "hrd/Debugger/ResolverDebugger.h"
#include "hrd/IR/HIR/HIRBuilder.h"
#include "hrd/IR/HIR/HIRHelper.h"
#include "hrd/IR/HIR/HIRLinker.h"
#include "hrd/IR/HIR/HIRProgram.h"
#include "hrd/IR/HIR/HIRVerifier.h"
#include "hrd/IR/MIR/MIRBuilder.h"
#include "hrd/IR/MIR/MIRProgram.h"
#include "hrd/IR/llvmIR/llvmCodegen.h"
#include "hrd/Lexer.h"
#include "hrd/Parser.h"
#include "hrd/SemanticAnalyzer.h"
#include "hrd/SemanticAnalyzer/SymbolTable.h"
#include "hrd/SemanticAnalyzer/Verifier.h"
#include "hrd/SourceSpan.h"
#include "hrd/compiler/CommandLineParser.h"
#include "hrd/compiler/CompilerContexts.h"
#include "hrd/compiler/ProjectLoader.h"
#include "hrd/util/diagnostic/DiagnosticEngine.h"
#include "hrd/util/diagnostic/DiagnosticRenderer.h"

#include <iostream>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <magic_enum/magic_enum.hpp>
#include <memory>
#include <ostream>
#include <vector>

using namespace std;

#ifndef NDEBUG
#define HGM_DEBUG 1
#else
#define HGM_DEBUG 0
#endif

namespace {

bool hasClangXX() {
#ifdef _WIN32
  int result = std::system("where clang++ > nul 2> nul");
#else
  int result = std::system("command -v clang++ > /dev/null 2>&1");
#endif
  return result == 0;
}

inline std::string_view tokenToString(TKind kind) {
  auto name = magic_enum::enum_name(kind);
  return name.empty() ? "UNKNOWN" : name;
}
} // namespace

CompilerDriver::CompilerDriver()
    : engine(DiagnosticEngine(
          make_unique<TerminalDiagnosticRenderer>(std::cout))) {}

bool CompilerDriver::loadInput(int argc, char **argv) {
  if (argc < 2) {
    cerr << "사용법: hgm <프로젝트디렉토리> [매개변수]" << endl;
    return false;
  }

  if (!parseOptions(argc, argv)) {
    return false;
  }

  return true;
}

bool CompilerDriver::parseOptions(int argc, char **argv) {

  CommandLineParser parser;
  auto invocation = parser.parse(argc, argv);

  if (!invocation.has_value()) {

    return false;
  }
  options = invocation->options;
  ProjectLoader loader;

  auto load = loader.load(invocation->projectRoot);

  if (load.has_value()) {
    projectInput = load.value();
    return true;
  }

  return false;
}

int CompilerDriver::run(int argc, char **argv) {

  if (!loadInput(argc, argv)) {
    return 1;
  }

  if (!hasClangXX()) {
    std::cerr << "link failed: clang++ not found\n";
    std::cerr
        << "please install clang++ and make sure it is available in PATH\n";
    return 1;
  }

  if (projectInput.sources.empty()) {
    std::cerr << "로드된 소스가 없습니다." << std::endl;
    return 1;
  }

  if (!runLexer()) {
    return 1;
  }
  if (!runParser()) {
    return 1;
  }
  if (!runSemantic()) {
    return 1;
  }
  if (!runHIR()) {
    return 1;
  }
  if (!runMIR()) {
    return 1;
  }
  if (!runCodegen()) {
    return 1;
  }
  if (!linkExecutable()) {
    return 1;
  }

  return 0;
}

bool CompilerDriver::runLexer() {
  std::cout << "project root : " << projectInput.rootPath << "\n";
  std::cout << "project file : " << projectInput.projectFilePath << "\n";
  std::cout << "src root     : " << projectInput.srcPath << "\n";
  std::cout << "source count : " << projectInput.sources.size() << "\n";

  for (auto const &i : projectInput.sources) {
    LexerContext context = {i, engine};
    Lexer lexer(context);
    try {
      storage.tokenStreams.push_back(lexer.lexing());
    } catch (std::runtime_error &e) {
#if HGM_DEBUG
      cout << Color::RED << "error occur while lexing\n" << Color::RESET;
#endif
      return false;
    }
  }

  if (options.dumpLexer) {
    cout << "===== Lexing result =====" << endl;
    for (auto const &stream : storage.tokenStreams) {

      for (const auto &tok : stream.tokens) {
        std::cout << "[" << tokenToString(tok.kind) << "] " << tok.text
                  << " (line " << tok.span.lineStart << ", col "
                  << tok.span.colStart << ")" << std::endl;
      }
    }
    cout << "=========================" << endl;
  }
  return true;
}

bool CompilerDriver::runParser() {

  storage.program = make_unique<Program>();

  for (const auto &s : storage.tokenStreams) {
    ParserContext context = {s, engine};
    Parser parser(context);
    try {
      storage.program->sources.push_back(
          make_shared<SourceFile>(s.path, parser.parse()));
    } catch (std::runtime_error &e) {
#if HGM_DEBUG
      cout << Color::RED << "error occur while parsing\n";
#endif
      cout << Color::RESET << e.what() << "\n";
      return false;
    }
  }

  if (options.dumpParser) {
    ParserDebugger pd;
    cout << "===== parsing result =====" << endl;
    for (auto &s : storage.program->sources) {
      for (auto &a : s->decls) {
        a->accept(&pd);
      }
    }
    cout << "=========================" << endl;
  }
  return true;
}

bool CompilerDriver::runSemantic() {

  storage.table = SymbolTable();
  SemanContext context = {storage.program.get(), storage.table, engine};

  SemanticAnalyzer analyzer(context);

  try {
    analyzer.build();
  } catch (std::runtime_error &e) {
#if HGM_DEBUG
    cout << Color::RED << "error occur while building\n";
#endif
    cout << Color::RESET << e.what() << "\n";
    return false;
  }

  if (options.dumpBuilder) {
    cout << "===== Building result =====" << endl;
    BuilderDebugger bd(context.table.getCurrent());
    bd.debug(context.table.rootScope.get());
    bd.debug();
    cout << "=========================" << endl;
  }

  try {
    analyzer.link();
  } catch (std::runtime_error &e) {
#if HGM_DEBUG
    cout << Color::RED << "error occur while linking\n";
#endif
    cout << Color::RESET << e.what() << "\n";
    return false;
  }

  try {
    analyzer.resolve();
  } catch (std::runtime_error &e) {
#if HGM_DEBUG
    cout << Color::RED << "error occur while resolving\n";
#endif
    cout << Color::RESET << e.what() << "\n";
    return false;
  }

  if (options.dumpResolver) {
    cout << "===== Resolving result =====" << endl;
    ResolverDebugger rd(context.table.getCurrent());
    rd.debug(context.table.rootScope.get());
    rd.debug(context.table.main->rootScope.get());
    rd.debug();
    cout << "=========================" << endl;
  }

  Verifier verifier(storage.program.get());

  try {
    verifier.verify();
  } catch (std::runtime_error &e) {
#if HGM_DEBUG
    cout << Color::RED << "error occur while verifying\n";
#endif
    cout << Color::RESET << e.what() << "\n";
    return false;
  }

  return true;
}

bool CompilerDriver::runHIR() {

  storage.hirProgram =
      make_unique<HIRProgram>(SourceSpan{"[program]", 0}, &storage.table);

  HIRContext context = {storage.hirProgram.get(), storage.table};

  try {
    for (auto &s : storage.program->sources) {
      HIRLinker linker(context, s.get());
      context.program->sources.push_back(linker.link());
    }

    HIRHelper::linkRoot(context.program);
    HIRHelper::linkSecondPass(context.program);
    for (auto &s : context.program->sources) {
      HIRBuilder builder(context, s.get());
      builder.build();
    }

  } catch (std::runtime_error &e) {
#if HGM_DEBUG
    cout << Color::RED << "error occur while hir building\n";
#endif
    cout << Color::RESET << e.what() << "\n";
    return false;
  }

  if (options.dumpHIR) {
    cout << "===== HIR result =====" << endl;
    HIRDebugger hirDebugger = HIRDebugger(context.program);
    hirDebugger.debug();
    cout << "=========================" << endl;
  }

  try {
    HIRVerifier hirVerifer(context.program);
    hirVerifer.verify();
  } catch (std::runtime_error &e) {
#if HGM_DEBUG
    cout << Color::RED << "error occur while hir verifying\n";
#endif
    cout << Color::RESET << e.what() << "\n";
    return false;
  }

  return true;
}

bool CompilerDriver::runMIR() {
  storage.mirProgram = make_unique<MIRProgram>();

  MIRContext context = {storage.hirProgram.get(), storage.mirProgram.get(),
                        storage.table};

  try {
    MIRBuilder mirBuilder(context);
    mirBuilder.build();
  } catch (std::runtime_error &e) {
#if HGM_DEBUG
    cout << Color::RED << "error occur while mir building\n";
#endif
    cout << Color::RESET << e.what() << "\n";
    return false;
  }

  if (options.dumpMIR) {
    cout << "===== MIR result =====" << endl;
    MIRDebugger mirDebugger = MIRDebugger(context.mirProgram);
    mirDebugger.debug();
    MIRGraphvizDebugger graphDebugger = MIRGraphvizDebugger(context.mirProgram);
    graphDebugger.debug();
    cout << "=========================" << endl;
  }
  return true;
}

bool CompilerDriver::runCodegen() {

  CodegenContext context = {storage.mirProgram.get(), storage.table};

  llvmCodegen codegen(context);
  try {

    codegen.generate();

    std::error_code ec;
    llvm::raw_fd_ostream out("out.ll", ec);

    if (ec) {
      throw std::runtime_error("failed to open out.ll: " + ec.message());
    }

    codegen.llvmModule->print(out, nullptr);
    out.flush();
    if (llvm::verifyModule(*codegen.llvmModule, &llvm::errs())) {
      throw std::runtime_error("invalid llvm module");
    }

#if HGM_DEBUG
    llvm::outs() << "\n===== LLVM IR =====\n";
    codegen.llvmModule->print(llvm::outs(), nullptr);
    llvm::outs().flush();
#endif

  } catch (std::runtime_error &e) {
#if HGM_DEBUG
    cout << Color::RED << "error occur while codegen\n";
#endif
    cout << Color::RESET << e.what() << "\n";
    return false;
  }

  return true;
}

bool CompilerDriver::linkExecutable() {

  int result = -1;

#if HGM_DEBUG

  result = std::system("clang++ out.ll "
                       "build/debug-asan/libhrd_runtime.a "
                       "-fsanitize=address,undefined "
                       "-I./include "
                       "-o main");

#else

  result = std::system("clang++ out.ll "
                       "build/release/libhrd_runtime.a "
                       "-I./include "
                       "-O2 "
                       "-o main");

#endif

  if (result != 0) {
    std::cerr << "link failed\n";
    return false;
  }

  cout << "end compile\n";
  return true;
}
