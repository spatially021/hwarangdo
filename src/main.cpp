#include "hrd/compiler/CompilerDriver.h"
#include <llvm/Support/TargetSelect.h>

int main(int argc, char *argv[]) {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmParser();
  llvm::InitializeNativeTargetAsmPrinter();
  CompilerDriver driver = CompilerDriver();
  return driver.run(argc, argv);
}
