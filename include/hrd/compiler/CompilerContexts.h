#pragma once

#include <memory>
#include <vector>

// AST
class Program;
class TokenStream;
class Expr;

// IR
class HIRProgram;
class MIRProgram;

// Compiler
class InputSource;

// Metadata
struct ModuleMeta;

// Semantic Analyzer
class Module;
class SymbolTable;
class SymbolRegistry;
struct Scope;

// Diagnostic
class DiagnosticEngine;

struct LexerContext {
  const InputSource &source;
  DiagnosticEngine &engine;
};

struct ParserContext {
  TokenStream &tokenStream;
  DiagnosticEngine &engine;
};

struct ImportedContext {
  ModuleMeta &meta;
  Module *module;
  SymbolTable &table;
  std::vector<std::shared_ptr<Expr>> &imported;
  Scope *scope;
};

struct SemanContext {
  Program *program = nullptr;
  SymbolTable &table;
  DiagnosticEngine &engine;
  bool isCompile;
};

struct BuilderContext {
  SymbolTable &table;
  DiagnosticEngine &engine;
};

struct LinkerContext {
  SymbolTable &table;
  DiagnosticEngine &engine;
};

struct ResolverContext {
  SymbolTable &table;
  DiagnosticEngine &engine;
};

struct VerifierContext {
  Program *program;
  DiagnosticEngine &engine;
};

struct HIRContext {
  HIRProgram *program = nullptr;
  DiagnosticEngine &engine;
  SymbolTable &table;
};

struct HIRVerifierContext {
  HIRProgram *program = nullptr;
  DiagnosticEngine &engine;
};

struct MIRContext {
  HIRProgram *hirProgram = nullptr;
  MIRProgram *mirProgram = nullptr;
  SymbolTable &table;
};

struct CodegenContext {
  MIRProgram *program = nullptr;
  SymbolTable &table;
  bool isCompile;
};