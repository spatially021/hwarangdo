#ifndef NDEBUG
#define HRD_DEBUG 1
#else
#define HRD_DEBUG 0
#endif

#include "hrd/compiler/CompilerDriver.h"
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
#include "hrd/Lexer.h"
#include "hrd/MetaData/MetaBuilder.h"
#include "hrd/MetaData/MetaData.h"
#include "hrd/MetaData/MetaReader.h"
#include "hrd/MetaData/MetaWriter.h"
#include "hrd/Parser.h"
#include "hrd/SemanticAnalyzer.h"
#include "hrd/SemanticAnalyzer/Module.h"
#include "hrd/SemanticAnalyzer/SymbolTable/SymbolTable.h"
#include "hrd/SemanticAnalyzer/Verifier.h"
#include "hrd/SourceSpan.h"
#include "hrd/compiler/CommandLineParser.h"
#include "hrd/compiler/CompilerContexts.h"
#include "hrd/compiler/CompilerLinker.h"
#include "hrd/compiler/CompilerSDK.h"
#include "hrd/compiler/LinkInput.h"
#include "hrd/compiler/ProjectLoader.h"
#include "hrd/diagnostic/Diagnostic.h"
#include "hrd/diagnostic/DiagnosticEngine.h"
#include "hrd/diagnostic/DiagnosticRenderer.h"

#if HRD_DEBUG
#include "hrd/Debugger/BuilderDebugger.h"
#include "hrd/Debugger/HIRDebugger.h"
#include "hrd/Debugger/MIRDebugger/MIRDebuuger.h"
#include "hrd/Debugger/MIRDebugger/MIRGraphvizDebugger.h"
#include "hrd/Debugger/ParserDebugger.h"
#include "hrd/Debugger/ResolverDebugger.h"
#endif

#include <cstddef>
#include <fstream>
#include <iostream>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <magic_enum/magic_enum.hpp>
#include <memory>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace std;

namespace {

void printHelp() {
  std::cout << "HwarangDo Compiler\n"
            << "\n"
            << "Usage:\n"
            << "  hrd <command> [project]\n"
            << "\n"
            << "Commands:\n"
            << "  build      Build the project and create an executable\n"
            << "  compile    Compile the project without linking\n"
            << "\n"
            << "Options:\n"
            << "  -h, --help Show this help message\n"
            << "\n"
            << "Arguments:\n"
            << "  project    Project directory (default: current directory)\n"
            << "\n"
            << "Examples:\n"
            << "  hrd build\n"
            << "  hrd build ./my-project\n"
            << "  hrd compile\n";
}

void printStage(std::size_t current, std::size_t total, std::string_view name) {
#if !HRD_DEBUG
  std::cout << '\r' << "\033[2K"; // 현재 줄 전체 삭제
#endif
  std::cout << '[' << current << '/' << total << "] " << name << "...";
  std::cout.flush();
}

} // namespace

namespace {
#if HRD_DEBUG
inline std::string_view tokenToString(TKind kind) {
  auto name = magic_enum::enum_name(kind);
  return name.empty() ? "UNKNOWN" : name;
}
#endif
} // namespace

CompilerDriver::CompilerDriver()
    : engine(DiagnosticEngine(
          make_unique<TerminalDiagnosticRenderer>(std::cout))) {}

bool CompilerDriver::loadInput(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "Usage: hrd <command> [project]\n"
              << "\n"
              << "Commands:\n"
              << "  build      Build the project and create an executable\n"
              << "  compile    Compile the project without linking\n"
              << "\n"
              << "Project defaults to the current directory.\n";

    return false;
  }
  if (!parseOptions(argc, argv)) {
    return false;
  }

  return true;
}

bool CompilerDriver::parseOptions(int argc, char **argv) {

  CommandLineParser parser;
  auto result = parser.parse(argc, argv);

  if (!result.result || !result.invocation.has_value()) {
    switch (result.kind) {

    case FailKind::None: {
      break;
    }
    case FailKind::Help: {
      printHelp();
      break;
    }
    case FailKind::Unknown:
      break;
    }
    return false;
  }

  invocation = result.invocation.value();
  options = invocation.options;
  ProjectLoader loader;

  auto load = loader.load(invocation.projectRoot);

  if (load.has_value()) {
    projectInput = load.value();
    return true;
  }

  return false;
}

int CompilerDriver::run(int argc, char **argv) {

  try {
    if (!loadInput(argc, argv)) {
      return 1;
    }
  } catch (std::runtime_error &e) {
#if HRD_DEBUG
    cout << Color::RED << "error occur while input process\n" << Color::RESET;
#endif
    return false;
  }

  if (projectInput.sources.empty()) {
    std::cerr << "로드된 소스가 없습니다." << std::endl;
    return 1;
  }

  string moudleName = "";

  switch (projectInput.config.status) {

  case ModuleConfigStatus::Loaded: {
    moudleName = projectInput.config.name;
    break;
  }
  case ModuleConfigStatus::Missing: {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_D001);
    dia.labels = {
        {nullopt, "this file has no module declaration", true},
    };
    dia.notes = {
        "the file will be treated as part of the current project module",
    };
    dia.helps = {
        "add a module declaration at the top of the file",
    };
    engine.emit(dia);
    moudleName = "moudle";
    break;
  }
  case ModuleConfigStatus::Invalid: {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_D002);
    dia.labels = {
        {nullopt, "invalid TOML syntax appears here", true},
    };
    dia.notes = {
        "the project configuration could not be loaded",
    };
    dia.helps = {
        "fix the TOML syntax in the project configuration file",
    };
    engine.emit(dia);
    return 1;
  } break;
  }

  auto sdk = SDKLocator::locate();
  if (!sdk.has_value()) {
    return 1;
  }
  storage.sdk = sdk.value();

  storage.module =
      make_unique<Module>(moudleName, projectInput.projectFilePath);
  storage.table.registry.setModule(storage.module.get());
  constexpr std::size_t stageCount = 8;
  size_t current = 1;
  std::cout << "Building project: " << projectInput.config.name << "\n\n";
  std::cout << "project root : " << projectInput.rootPath << "\n";
  std::cout << "project file : " << projectInput.projectFilePath << "\n";
  std::cout << "src root     : " << projectInput.srcPath << "\n";
  std::cout << "source count : " << projectInput.sources.size() << "\n\n";

  printStage(current++, stageCount, "Lexing");
  if (!runLexer()) {
    return 1;
  }

  printStage(current++, stageCount, "Parsing");
  if (!runParser()) {
    return 1;
  }

  printStage(current++, stageCount, "load library");
  if (!loadLib()) {
    return 1;
  }

  printStage(current++, stageCount, "Semantic analysis");
  if (!runSemantic()) {
    return 1;
  }

  storage.table.registry.setCurrentFile(nullptr);

  printStage(current++, stageCount, "Metadata");
  if (invocation.options.isCompile)
    if (!runMeta()) {
      return 1;
    }

  printStage(current++, stageCount, "HIR");
  if (!runHIR()) {
    return 1;
  }

  printStage(current++, stageCount, "MIR");
  if (!runMIR()) {
    return 1;
  }

  const auto objectPath =
      projectInput.rootPath / "build" / "obj" / (moudleName + ".o");

  const auto metaPath =
      projectInput.rootPath / "build" / "obj" / (moudleName + ".hmeta");

  printStage(7, stageCount, "Code generation");
  if (!runCodegen(objectPath)) {
    return 1;
  }

  if (invocation.options.isCompile)
    if (!writeMeta(metaPath)) {
      return 1;
    }

  std::cout << '\r' << "\033[2K" << Color::GREEN << "[7/7] Compilation complete"
            << Color::RESET << '\n';

  if (!invocation.options.isCompile) {
    std::cout << "[link] Linking...";
    std::cout.flush();

    LinkInput input;

    input.input = objectPath;
    input.runtime = storage.sdk.runtime;
    input.output = projectInput.rootPath / "build" / "bin" / moudleName;
    for (auto &[_, import] : importedModules) {
      input.libraries.push_back(import.objectPath);
    }
    CompilerLinker linker;

    if (!linker.link(input)) {
      return 1;
    }

    std::error_code ec;
    fs::remove(objectPath, ec);

    if (ec) {
#if HRD_DEBUG
      std::cerr << "warning: failed to remove temporary object: "
                << ec.message() << '\n';
#endif
    }

    std::cout << " done\n";

    std::cout << "\nBuild completed: " << input.output << '\n';
  } else {
    std::cout << "\nCompile completed: " << objectPath << '\n';
  }
  return 0;
}

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
    ImportedContext ctx = {imported.meta, raw, storage.table, storage.imported,
                           storage.libTopLevel.get()};
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
  MetaBuilder builder = MetaBuilder(storage.table);
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

bool CompilerDriver::runCodegen(const std::filesystem::path &objectPath) {

  CodegenContext context = {storage.mirProgram.get(), storage.table,
                            invocation.options.isCompile};

  llvmCodegen codegen(context);

  try {
    codegen.generate();

    if (llvm::verifyModule(*codegen.llvmModule, &llvm::errs())) {
      throw std::runtime_error("invalid llvm module");
    }

#if HRD_DEBUG
    llvm::outs() << "\n===== LLVM IR =====\n";
    codegen.llvmModule->print(llvm::outs(), nullptr);
    llvm::outs().flush();
#endif

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

bool CompilerDriver::writeMeta(const std::filesystem::path &metaPath) {
  fs::create_directories(metaPath.parent_path());

  std::ofstream metaOut(metaPath);

  if (!metaOut) {
    Error::internal("failed to open metadata output: " + metaPath.string());
  }
  MetaWriter writer = MetaWriter();
  try {
    writer.write(storage.module->name, storage.moduleMeta, metaOut);
  } catch (const std::runtime_error &e) {
    std::cerr << e.what() << '\n';
    return false;
  } catch (const Failure &) {
    return false;
  }
  return true;
}

void CompilerDriver::readMeta(const std::filesystem::path &libPath) {
  for (const auto &entry : fs::directory_iterator(libPath)) {
    if (!entry.is_regular_file()) {
      continue;
    }

    if (entry.path().extension() != ".hmeta") {
      continue;
    }

    std::ifstream in(entry.path());

    if (!in) {
      Error::meta(entry.path().string(), " : failed to open metadata file");
    }

    MetaReader reader;
    MetaReadResult result = reader.read(in, entry.path());

#if HRD_DEBUG
    MetaWriter writer = MetaWriter(true);
    cout << "\n";
    writer.write(result.moduleName, result.meta, std::cout);
#endif

    auto object = invocation.projectRoot / "lib" / result.moduleName;

    object.replace_extension(".o");

    if (!fs::exists(object)) {
      Error::meta(entry.path().string(),
                  "object file not found for module: " + result.moduleName);
    }

    auto [it, inserted] =
        importedModules.emplace(result.moduleName, ImportedModule{
                                                       std::move(result.meta),
                                                       object,
                                                   });
    if (!inserted) {
      Error::meta(entry.path().string(),
                  "duplicated metadata module: " + result.moduleName);
    }
  }
}