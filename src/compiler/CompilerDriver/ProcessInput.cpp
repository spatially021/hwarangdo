#include "hrd/compiler/CommandLineParser.h"
#include "hrd/compiler/CompilerDriver.h"
#include "hrd/compiler/ProjectLoader.h"
#include <iostream>
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
} // namespace

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
