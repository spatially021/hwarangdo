#pragma once

#include "hrd/IR/MIR/MIRProgram.h"
#include <string>

class MIRGraphvizDebugger {
  MIRProgram *program = nullptr;

public:
  explicit MIRGraphvizDebugger(MIRProgram *p);

  void debug(const std::string &outDir = "dump/cfg");

private:
  void debugFunction(MIRFunction *func, const std::string &path);
  void writeNode(std::ostream &out, BasicBlock *block);
  void writeEdges(std::ostream &out, BasicBlock *block);
  void debugProgram(const string &all_path);
  void writeFunctionCluster(ostream &out, MIRFunction *func);
  void writeNode(ostream &out, MIRFunction *func, BasicBlock *block);
  void writeEdges(ostream &out, MIRFunction *func, BasicBlock *block);
  string blockNodeName(MIRFunction *func, BlockID id);

  std::string functionName(MIRFunction *func);
  std::string escapeDot(const std::string &s);
  void renderSvg(const std::string &dotPath, const std::string &svgPath);
  void renderPng(const std::string &dotPath, const std::string &svgPath);
};