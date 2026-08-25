#include "hrd/compiler/CommandLineParser.h"

#include <iostream>
#include <string>

LPresult CommandLineParser::parse(int argc, char **argv) {

  if (argc < 2) {
    return {false, std::nullopt, FailKind::Help};
  }

  CompilerInvocation invocation;

  const std::string command = argv[1];

  if (command == "build") {
    invocation.options.isCompile = false;
  } else if (command == "compile") {
    invocation.options.isCompile = true;

  } else if (command == "--help" || command == "-h") {
    return {false, std::nullopt, FailKind::Help};
  } else {
    std::cerr << "error: unknown command '" << command << "'\n"
              << "try 'hrd --help' for usage information\n";

    return {false, std::nullopt, FailKind::Unknown};
  }

  if (argc >= 3) {
    invocation.projectRoot = std::filesystem::path(argv[2]).lexically_normal();
  } else {
    invocation.projectRoot = std::filesystem::current_path().lexically_normal();
  }

  for (int i = 3; i < argc; ++i) {
    const std::string arg = argv[i];

    // TODO: option parsing
  }

  return {true, invocation, FailKind::None};
}