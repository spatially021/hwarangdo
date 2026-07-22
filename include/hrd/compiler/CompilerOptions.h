#pragma once

struct CompilerOptions {
  bool dumpLexer = false;
  bool dumpParser = false;
  bool dumpBuilder = false;
  bool dumpResolver = false;
  bool dumpHIR = false;
  bool dumpMIR = false;

  bool testMode = false;
};