#include "hrd/compiler/CompilerDriver.h"

int main(int argc, char *argv[]) {
  CompilerDriver driver = CompilerDriver();
  return driver.run(argc, argv);
}
