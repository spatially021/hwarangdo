#include "CompilerOptions.h"
#include "hrd/AST/Program.h"
#include "hrd/AST/TokenStream.h"
#include "hrd/IR/HIR/HIRProgram.h"
#include "hrd/IR/MIR/MIRProgram.h"
#include "hrd/Inputs.h"
#include "hrd/SemanticAnalyzer/SymbolTable.h"
#include "hrd/util/diagnostic/DiagnosticEngine.h"
#include <memory>
#include <vector>

struct CompilerStorage {
  std::vector<TokenStream> tokenStreams;
  unique_ptr<Program> program = nullptr;
  SymbolTable table;
  std::unique_ptr<HIRProgram> hirProgram = nullptr;
  std::unique_ptr<MIRProgram> mirProgram = nullptr;
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
  bool loadInput(int argc, char **argv);
  bool parseOptions(int argc, char **argv);

  bool runLexer();
  bool runParser();
  bool runSemantic();
  bool runHIR();
  bool runMIR();
  bool runCodegen();
  bool linkExecutable();
};