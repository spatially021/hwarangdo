#pragma once

#include "hrd/AST/Program.h"
#include "hrd/AST/TokenStream.h"
#include "hrd/IR/HIR/HIRProgram.h"
#include "hrd/IR/MIR/MIRProgram.h"
#include "hrd/Inputs.h"
#include "hrd/SemanticAnalyzer/SymbolTable.h"
#include "hrd/util/diagnostic/DiagnosticEngine.h"

struct LexerContext {
  const InputSource &source;
  DiagnosticEngine &engine;
};

struct ParserContext {
  const TokenStream &tokenStream;
  DiagnosticEngine &engine;
};

struct SemanContext {
  Program *program = nullptr;
  SymbolTable &table;
  DiagnosticEngine &engine;
};

struct BuilderContext {
  SymbolTable &table;
  DiagnosticEngine &engine;
};

struct HIRContext {
  HIRProgram *program = nullptr;
  SymbolTable &table;
};

struct MIRContext {
  HIRProgram *hirProgram = nullptr;
  MIRProgram *mirProgram = nullptr;
  SymbolTable &table;
};

struct CodegenContext {
  MIRProgram *program = nullptr;
  SymbolTable &table;
};