#include "CompilerOptions.h"
#include "hrd/AST/Program.h"
#include "hrd/AST/TokenStream.h"
#include "hrd/IR/HIR/HIRProgram.h"
#include "hrd/IR/MIR/MIRProgram.h"
#include "hrd/InitChecker/InitSummary.h"
#include "hrd/Inputs.h"
#include "hrd/MetaData/ImportedModule.h"
#include "hrd/MetaData/MetaData.h"
#include "hrd/SemanticAnalyzer/Module.h"
#include "hrd/SemanticAnalyzer/SymbolTable/SymbolTable.h"
#include "hrd/compiler/CompilerInvocation.h"
#include "hrd/compiler/CompilerSDK.h"
#include "hrd/diagnostic/DiagnosticEngine.h"
#include <memory>
#include <unordered_map>
#include <vector>

struct CompilerStorage {
  SDKPaths sdk;
  std::vector<TokenStream> tokenStreams;
  unique_ptr<Program> program = nullptr;
  SymbolTable table = SymbolTable();
  unique_ptr<Module> module = nullptr;
  vector<unique_ptr<Module>> modules;
  std::unique_ptr<HIRProgram> hirProgram = nullptr;
  std::unique_ptr<MIRProgram> mirProgram = nullptr;
  ModuleMeta moduleMeta;
  std::vector<shared_ptr<Expr>> imported;
  unique_ptr<Scope> libTopLevel = make_unique<Scope>();
  InitSummary summary;
};

class CompilerDriver {
public:
  CompilerDriver();
  int run(int argc, char **argv);

private:
  CompilerOptions options;
  ProjectInput projectInput;
  CompilerStorage storage;
  DiagnosticEngine engine;
  CompilerInvocation invocation;

  std::unordered_map<std::string, ImportedModule> importedModules;

  bool loadInput(int argc, char **argv);
  bool parseOptions(int argc, char **argv);

  bool runLexer();
  bool runParser();
  bool loadLib();
  bool runSemantic();
  bool runMeta();
  bool runHIR();
  bool runInitCheck();
  bool runMIR();
  bool runCodegen(const std::filesystem::path &objectPath);
  bool linkExecutable();
  bool writeMeta(const std::filesystem::path &metaPath);
  void readMeta(const std::filesystem::path &libPath);
};