#ifndef NDEBUG
#define HRD_DEBUG 1
#else
#define HRD_DEBUG 0
#endif

#include "hrd/AST/Program.h"
#include "hrd/Color.h"
#include "hrd/IR/HIR/HIRBuilder.h"
#include "hrd/IR/HIR/HIRHelper.h"
#include "hrd/IR/HIR/HIRLinker.h"
#include "hrd/IR/HIR/HIRProgram.h"
#include "hrd/IR/HIR/HIRVerifier.h"
#include "hrd/IR/MIR/MIRBuilder.h"
#include "hrd/IR/MIR/MIRProgram.h"
#include "hrd/IR/llvmIR/llvmCodegen.h"
#include "hrd/Imported/ImportedSymbolBuilder.h"
#include "hrd/InitChecker/InitChecker.h"
#include "hrd/Lexer.h"
#include "hrd/MetaData/MetaBuilder.h"
#include "hrd/MetaData/MetaData.h"
#include "hrd/Parser.h"
#include "hrd/SemanticAnalyzer.h"
#include "hrd/SemanticAnalyzer/Module.h"
#include "hrd/SemanticAnalyzer/SymbolTable/SymbolTable.h"
#include "hrd/SemanticAnalyzer/Verifier.h"
#include "hrd/Serialize/MIRSerializationAdapter.h"
#include "hrd/SourceSpan.h"
#include "hrd/compiler/CompilerContexts.h"
#include "hrd/compiler/CompilerDriver.h"

#if HRD_DEBUG
#include "hrd/Debugger/BuilderDebugger.h"
#include "hrd/Debugger/HIRDebugger.h"
#include "hrd/Debugger/MIRDebugger/MIRDebuuger.h"
#include "hrd/Debugger/MIRDebugger/MIRGraphvizDebugger.h"
#include "hrd/Debugger/ParserDebugger.h"
#include "hrd/Debugger/ResolverDebugger.h"
#include <llvm/Support/FileSystem.h>
#endif

#include <iostream>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <magic_enum/magic_enum.hpp>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
#if HRD_DEBUG
inline std::string_view tokenToString(TKind kind) {
  auto name = magic_enum::enum_name(kind);
  return name.empty() ? "UNKNOWN" : name;
}
#endif
} // namespace

bool CompilerDriver::runLexer() {
  for (auto const &i : projectInput.sources) {
    LexerContext context = {i, engine};
    Lexer lexer(context);
    try {
      storage.tokenStreams.push_back(lexer.lexing());
    } catch (std::runtime_error &e) {
#if HRD_DEBUG
      cout << Color::RED << "error occur while lexing\n" << Color::RESET;
#endif
      return false;
    } catch (Failure &f) {
      return false;
    }
  }

#if HRD_DEBUG
  cout << "===== Lexing result =====" << endl;
  for (auto const &stream : storage.tokenStreams) {

    for (const auto &tok : stream.tokens) {
      std::cout << "[" << tokenToString(tok.kind) << "] " << tok.text
                << " (line " << tok.span.lineStart << ", col "
                << tok.span.colStart << ")" << std::endl;
    }
  }
  cout << "=========================" << endl;
#endif

  return true;
}

bool CompilerDriver::runParser() {

  storage.program = make_unique<Program>();

  for (auto &s : storage.tokenStreams) {
    ParserContext context = {s, engine};
    Parser parser(context);
    try {
      storage.program->sources.push_back(
          make_shared<SourceFile>(s.path, s.locgicalPath, parser.parse()));
    } catch (std::runtime_error &e) {
#if HRD_DEBUG
      cout << Color::RED << "error occur while parsing\n";
#endif
      cout << Color::RESET << e.what() << "\n";
      return false;
    } catch (Failure &f) {
      return false;
    }
  }

#if HRD_DEBUG
  {
    ParserDebugger pd;
    cout << "===== parsing result =====" << endl;
    for (auto &s : storage.program->sources) {
      for (auto &a : s->decls) {
        a->accept(&pd);
      }
    }
    cout << "=========================" << endl;
  }
#endif
  return true;
}

bool CompilerDriver::loadLib() {
  try {
    const auto lib = projectInput.rootPath / "lib";
    readMeta(lib);
  } catch (std::runtime_error &e) {
#if HRD_DEBUG
    cout << Color::RED << "error occur while load library\n";
#endif
    cout << Color::RESET << e.what() << "\n";
    return false;
  } catch (Failure &f) {
    return false;
  }

  for (auto lib : importedModules) {
    auto name = lib.first;
    auto imported = lib.second;

    unique_ptr<Module> module = make_unique<Module>(name, imported.objectPath);
    auto raw = module.get();

    storage.modules.push_back(std::move(module));
    storage.table.registry.addModule(name, raw);
    ImportedContext ctx = {imported.meta,
                           raw,
                           storage.table,
                           storage.imported,
                           storage.libTopLevel.get(),
                           storage.summary};
    ImportedSymbolBuilder builder(ctx);
    try {
      builder.run();
    } catch (std::runtime_error &e) {
#if HRD_DEBUG
      cout << Color::RED << "error occur while build imported\n";
#endif
      cout << Color::RESET << e.what() << "\n";
      return false;
    } catch (Failure &f) {
      return false;
    }
  }

  return true;
}

bool CompilerDriver::runSemantic() {

  storage.table.moudle = storage.module.get();
  SemanContext context = {storage.program.get(), storage.table, engine,
                          invocation.options.isCompile};

  SemanticAnalyzer analyzer(context);

  try {
    analyzer.build();
  } catch (std::runtime_error &e) {
#if HRD_DEBUG
    cout << Color::RED << "error occur while building\n";
#endif
    cout << Color::RESET << e.what() << "\n";
    return false;
  } catch (Failure &f) {
    return false;
  }

#if HRD_DEBUG
  {
    cout << "===== Building result =====" << endl;
    BuilderDebugger bd(context.table.scopeManger.current());
    bd.debug(context.table.scopeManger.getRootScope());
    bd.debug();
    cout << "=========================" << endl;
  }
#endif

  try {
    analyzer.import();
  } catch (std::runtime_error &e) {
#if HRD_DEBUG
    cout << Color::RED << "error occur while import\n";
#endif
    cout << Color::RESET << e.what() << "\n";
    return false;
  } catch (Failure &f) {
    return false;
  }
#if HRD_DEBUG
  cout << "==============end import===========\n";
#endif
  try {
    analyzer.link();
  } catch (std::runtime_error &e) {
#if HRD_DEBUG
    cout << Color::RED << "error occur while linking\n";
#endif
    cout << Color::RESET << e.what() << "\n";
    return false;
  } catch (Failure &f) {
    return false;
  }

  try {
    analyzer.resolve();
  } catch (std::runtime_error &e) {
#if HRD_DEBUG
    cout << Color::RED << "error occur while resolving\n";
#endif
    cout << Color::RESET << e.what() << "\n";
    return false;
  } catch (Failure &f) {
    return false;
  }

#if HRD_DEBUG
  {
    cout << "===== Resolving result =====" << endl;
    ResolverDebugger rd(storage.table.scopeManger.getTopLevelScope());
    rd.debug();
    cout << "=========================" << endl;
  }
#endif

  VerifierContext vContext = {storage.program.get(), engine};
  Verifier verifier(vContext);

  try {
    verifier.verify();
  } catch (std::runtime_error &e) {
#if HRD_DEBUG
    cout << Color::RED << "error occur while verifying\n";
#endif
    cout << Color::RESET << e.what() << "\n";
    return false;
  } catch (Failure &f) {
    return false;
  }

  return true;
}

bool CompilerDriver::runMeta() {
  MetaBuilderContext ctx = {storage.table, storage.summary};
  MetaBuilder builder = MetaBuilder(ctx);
  try {
    storage.moduleMeta = builder.build();
  } catch (std::runtime_error &e) {
#if HRD_DEBUG
    cout << Color::RED << "error occur while meta build\n";
#endif
    cout << Color::RESET << e.what() << "\n";
    return false;
  } catch (Failure &f) {
    return false;
  }

  return true;
}

bool CompilerDriver::runHIR() {

  storage.hirProgram =
      make_unique<HIRProgram>(SourceSpan({"[program]", 0}), storage.table);
  storage.hirProgram->rootScope = storage.table.scopeManger.getRootScope();
  HIRContext context = {storage.hirProgram.get(), engine, storage.table};

  try {
    for (auto &s : storage.program->sources) {
      HIRLinker linker(context, s.get());
      context.program->sources.push_back(linker.link());
    }

    HIRHelper::linkSecondPass(context.program);
    for (auto &s : context.program->sources) {
      HIRBuilder builder(context, s.get());
      builder.build();
    }

  } catch (std::runtime_error &e) {
#if HRD_DEBUG
    cout << Color::RED << "error occur while hir building\n";
#endif
    cout << Color::RESET << e.what() << "\n";
    return false;
  } catch (Failure &f) {
    return false;
  }

#if HRD_DEBUG
  {
    cout << "===== HIR result =====" << endl;
    HIRDebugger hirDebugger = HIRDebugger(context.program);
    hirDebugger.debug();
    cout << "=========================" << endl;
  }
#endif

  try {
    HIRVerifierContext ctx = {storage.hirProgram.get(), engine};
    HIRVerifier hirVerifer(ctx);
    hirVerifer.verify();
  } catch (std::runtime_error &e) {
#if HRD_DEBUG
    cout << Color::RED << "error occur while hir verifying\n";
#endif
    cout << Color::RESET << e.what() << "\n";
    return false;
  } catch (Failure &f) {
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
#if HRD_DEBUG
    cout << Color::RED << "error occur while mir building\n";
#endif
    cout << Color::RESET << e.what() << "\n";
    return false;
  } catch (Failure &f) {
    return false;
  }

#if HRD_DEBUG
  {
    cout << "===== MIR result =====" << endl;
    MIRDebugger mirDebugger = MIRDebugger(context.mirProgram);
    mirDebugger.debug();
    MIRGraphvizDebugger graphDebugger = MIRGraphvizDebugger(context.mirProgram);
    graphDebugger.debug();
    cout << "=========================" << endl;
  }
#endif
  return true;
}

bool CompilerDriver::runInitCheck() {

  try {
    InitChecerContext ctx = {storage.hirProgram.get(), engine, storage.summary};
    InitChecker checker = InitChecker(ctx);
    checker.check();
    storage.summary = checker.getSummary();
  } catch (std::runtime_error &e) {
#if HRD_DEBUG
    cout << Color::RED << "error occur while init-check\n";
#endif
    cout << Color::RESET << e.what() << "\n";
    return false;
  } catch (Failure &f) {
    return false;
  }
  return true;
}

bool CompilerDriver::runCodegen(const std::filesystem::path &objectPath) {

  CodegenContext context = {storage.mirProgram.get(), storage.table,
                            invocation.options.isCompile};

  llvmCodegen codegen(context);

  try {
    codegen.generate();
#if HRD_DEBUG
    std::error_code ec;
    llvm::raw_fd_ostream out("hwarangdo.ll", ec, llvm::sys::fs::OF_Text);

    if (ec) {
      Error::internal("failed to open LLVM IR output file: " + ec.message());
    }

    codegen.llvmModule->print(out, nullptr);
    out.flush();
#endif

    if (llvm::verifyModule(*codegen.llvmModule, &llvm::errs())) {
      throw std::runtime_error("invalid llvm module");
    }

    if (!codegen.emitObject(objectPath)) {
      throw std::runtime_error("failed to emit object file");
    }

  } catch (const std::runtime_error &e) {
    std::cerr << e.what() << '\n';
    return false;
  } catch (const Failure &) {
    return false;
  }

  return true;
}