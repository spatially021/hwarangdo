#include "hrd/Debugger/MIRDebugger/MIRGraphvizDebugger.h"
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <type_traits>

using namespace std;

MIRGraphvizDebugger::MIRGraphvizDebugger(MIRProgram *p) : program(p) {}
void MIRGraphvizDebugger::debug(const string &outDir) {
  std::filesystem::create_directories(outDir);

  for (auto &entry : std::filesystem::directory_iterator(outDir)) {
    std::filesystem::remove(entry.path());
  }

  for (auto &func : program->functions) {
    string name = functionName(func.get());
    string dotPath = outDir + "/" + name + ".dot";
    string svgPath = outDir + "/" + name + ".svg";

    debugFunction(func.get(), dotPath);
    renderSvg(dotPath, svgPath);
  }

  string allDotPath = outDir + "/all.dot";
  string allSvgPath = outDir + "/all.svg";
  debugProgram(allDotPath);
  renderSvg(allDotPath, allSvgPath);
}

void MIRGraphvizDebugger::debugFunction(MIRFunction *func, const string &path) {
  ofstream out(path);

  if (!out.is_open()) {
    cerr << "failed to open graphviz output: " << path << "\n";
    return;
  }

  out << "digraph CFG {\n";
  out << "  graph [rankdir=TB];\n";
  out << "  node [shape=box, fontname=\"Consolas\"];\n";
  out << "  edge [fontname=\"Consolas\"];\n\n";

  out << "  label=\"" << escapeDot(functionName(func)) << "\";\n";
  out << "  labelloc=\"t\";\n\n";

  for (auto &block : func->blocks) {
    writeNode(out, block.get());
  }

  out << "\n";

  for (auto &block : func->blocks) {
    writeEdges(out, block.get());
  }

  out << "}\n";
}

void MIRGraphvizDebugger::writeNode(ostream &out, BasicBlock *block) {
  out << "  B" << block->id << " [label=\"";
  out << "block #" << block->id << "\\l";
  out << "stmts: " << block->stmts.size() << "\\l";
  out << "\"];\n";
}

void MIRGraphvizDebugger::writeEdges(ostream &out, BasicBlock *block) {
  std::visit(
      [&](auto &&t) {
        using T = std::decay_t<decltype(t)>;

        if constexpr (std::is_same_v<T, std::monostate>) {
          return;
        }

        else if constexpr (std::is_same_v<T, GotoTerminator>) {
          out << "  B" << block->id << " -> B" << t.targetBlock << ";\n";
        }

        else if constexpr (std::is_same_v<T, BranchTerminator>) {
          out << "  B" << block->id << " -> B" << t.trueBlock
              << " [label=\"true\"];\n";

          out << "  B" << block->id << " -> B" << t.falseBlock
              << " [label=\"false\"];\n";
        }

        else if constexpr (std::is_same_v<T, ReturnTerminator>) {
          return;
        }

        else if constexpr (std::is_same_v<T, SwitchTerminator>) {
          for (auto &c : t.cases) {
            out << "  B" << block->id << " -> B" << c.target
                << " [label=\"case\"];\n";
          }

          out << "  B" << block->id << " -> B" << t.defaultTarget
              << " [label=\"default\"];\n";
        }
      },
      block->terminator);
}

string MIRGraphvizDebugger::functionName(MIRFunction *func) {
  string name;

  if (func->owner != nullptr)
    name += func->owner->name + ".";

  if (func->symbol != nullptr)
    name += func->symbol->name;
  else
    name += "unknown";

  for (char &c : name) {
    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.'))
      c = '_';
  }

  return name;
}
string MIRGraphvizDebugger::escapeDot(const string &s) {
  string out;

  for (char c : s) {
    switch (c) {
    case '\\':
      out += "\\\\";
      break;
    case '"':
      out += "\\\"";
      break;
    case '\n':
      out += "\\n";
      break;
    default:
      out += c;
      break;
    }
  }

  return out;
}

void MIRGraphvizDebugger::renderSvg(const string &dotPath,
                                    const string &svgPath) {
  string cmd = "dot -Tsvg \"" + dotPath + "\" -o \"" + svgPath + "\"";
  int result = std::system(cmd.c_str());

  if (result != 0) {
    cerr << "Graphviz render failed: " << cmd << "\n";
  }
}

void MIRGraphvizDebugger::renderPng(const string &dotPath,
                                    const string &svgPath) {
  string cmd = "dot -Tpng \"" + dotPath + "\" -o \"" + svgPath + "\"";
  int result = std::system(cmd.c_str());

  if (result != 0) {
    cerr << "Graphviz render failed: " << cmd << "\n";
  }
}

void MIRGraphvizDebugger::debugProgram(const string &path) {
  ofstream out(path);

  if (!out.is_open()) {
    cerr << "failed to open graphviz output: " << path << "\n";
    return;
  }

  out << "digraph CFG_ALL {\n";
  out << "  graph [rankdir=TB, compound=true];\n";
  out << "  node [shape=box, fontname=\"Consolas\"];\n";
  out << "  edge [fontname=\"Consolas\"];\n\n";

  out << "  label=\"MIR Program CFG\";\n";
  out << "  labelloc=\"t\";\n\n";

  for (auto &func : program->functions) {
    writeFunctionCluster(out, func.get());
  }

  out << "}\n";
}

void MIRGraphvizDebugger::writeFunctionCluster(ostream &out,
                                               MIRFunction *func) {
  string funcName = functionName(func);
  string clusterName = funcName;

  for (char &c : clusterName) {
    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_'))
      c = '_';
  }

  out << "  subgraph cluster_" << clusterName << " {\n";
  out << "    label=\"" << escapeDot(funcName) << "\";\n";
  out << "    style=rounded;\n\n";

  for (auto &block : func->blocks) {
    writeNode(out, func, block.get());
  }

  out << "\n";

  for (auto &block : func->blocks) {
    writeEdges(out, func, block.get());
  }

  out << "  }\n\n";
}

string MIRGraphvizDebugger::blockNodeName(MIRFunction *func, BlockID id) {
  string name = functionName(func) + "_B" + std::to_string(id);

  for (char &c : name) {
    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_'))
      c = '_';
  }

  return name;
}

void MIRGraphvizDebugger::writeNode(ostream &out, MIRFunction *func,
                                    BasicBlock *block) {
  out << "    " << blockNodeName(func, block->id) << " [label=\"";
  out << "block #" << block->id << "\\l";
  out << "stmts: " << block->stmts.size() << "\\l";
  out << "\"];\n";
}

void MIRGraphvizDebugger::writeEdges(ostream &out, MIRFunction *func,
                                     BasicBlock *block) {
  std::visit(
      [&](auto &&t) {
        using T = std::decay_t<decltype(t)>;

        string from = blockNodeName(func, block->id);

        if constexpr (std::is_same_v<T, std::monostate>) {
          return;
        }

        else if constexpr (std::is_same_v<T, GotoTerminator>) {
          out << "    " << from << " -> " << blockNodeName(func, t.targetBlock)
              << ";\n";
        }

        else if constexpr (std::is_same_v<T, BranchTerminator>) {
          out << "    " << from << " -> " << blockNodeName(func, t.trueBlock)
              << " [label=\"true\"];\n";

          out << "    " << from << " -> " << blockNodeName(func, t.falseBlock)
              << " [label=\"false\"];\n";
        }

        else if constexpr (std::is_same_v<T, ReturnTerminator>) {
          return;
        }

        else if constexpr (std::is_same_v<T, SwitchTerminator>) {
          for (auto &c : t.cases) {
            out << "    " << from << " -> " << blockNodeName(func, c.target)
                << " [label=\"case\"];\n";
          }

          out << "    " << from << " -> "
              << blockNodeName(func, t.defaultTarget)
              << " [label=\"default\"];\n";
        }
      },
      block->terminator);
}
