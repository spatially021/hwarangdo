#include "hrd/compiler/CommandLineParser.h"

#include <iostream>
#include <string>

void CommandLineParser::enableAllDumps(CompilerOptions &options) {
  options.dumpLexer = true;
  options.dumpParser = true;
  options.dumpBuilder = true;
  options.dumpResolver = true;
  options.dumpHIR = true;
  options.dumpMIR = true;
}

std::optional<CompilerInvocation> CommandLineParser::parse(int argc,
                                                           char *argv[]) {
  if (argc < 2) {
    std::cerr << "usage: hgm <project-directory> [options]\n";
    return std::nullopt;
  }

  CompilerInvocation invocation;
  invocation.projectRoot = std::filesystem::path(argv[1]).lexically_normal();

  for (int i = 2; i < argc; ++i) {
    const std::string arg = argv[i];

    if (arg == "--lexer") {
      invocation.options.dumpLexer = true;
    } else if (arg == "--parser") {
      invocation.options.dumpParser = true;
    } else if (arg == "--builder") {
      invocation.options.dumpBuilder = true;
    } else if (arg == "--resolver") {
      invocation.options.dumpResolver = true;
    } else if (arg == "--hir") {
      invocation.options.dumpHIR = true;
    } else if (arg == "--mir") {
      invocation.options.dumpMIR = true;
    } else if (arg == "--all") {
      enableAllDumps(invocation.options);
    } else if (arg == "--test") {
      invocation.options.testMode = true;
    } else if (arg.size() >= 2 && arg[0] == '-' && arg[1] != '-') {
      for (std::size_t j = 1; j < arg.size(); ++j) {
        switch (arg[j]) {
        case 'l':
          invocation.options.dumpLexer = true;
          break;
        case 'p':
          invocation.options.dumpParser = true;
          break;
        case 'b':
          invocation.options.dumpBuilder = true;
          break;
        case 'r':
          invocation.options.dumpResolver = true;
          break;
        case 'h':
          invocation.options.dumpHIR = true;
          break;
        case 'm':
          invocation.options.dumpMIR = true;
          break;
        case 'a':
          enableAllDumps(invocation.options);
          break;
        case 't':
          invocation.options.testMode = true;
          break;
        default:
          std::cerr << "unknown option: -" << arg[j] << '\n';
          return std::nullopt;
        }
      }
    } else {
      std::cerr << "invalid argument: " << arg << '\n';
      return std::nullopt;
    }
  }

  return invocation;
}