#include "Color.h"
#include "Debugger/BuilderDebugger.h"
#include "Debugger/ParserDebugger.h"
#include "Debugger/ResolverDebugger.h"
#include "IR/Codegen.h"
#include "IR/MIR.h"
#include "Lexer.h"
#include "Parser.h"
#include "SemanticAnalyzer.h"
#include "SemanticAnalyzer/Verifier.h"
#include "Token.h"
#include <fstream>
#include <functional>
#include <iostream>
#include <magic_enum/magic_enum.hpp>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

using namespace std;

inline string_view tokenToString(TKind kind);

void tester() {
  MirModule module;

  auto fn = std::make_unique<MirFunction>();
  fn->name = "main";
  fn->returnType.kind = MirTypeKind::I32;

  auto bb = std::make_unique<BasicBlock>();

  // 40
  auto c1 = std::make_unique<ConstantIntValue>(40);
  MirValue *p1 = c1.get();

  // 2
  auto c2 = std::make_unique<ConstantIntValue>(2);
  MirValue *p2 = c2.get();

  // 40 + 2
  auto add = std::make_unique<BinaryOpValue>(BinaryOpKind::Div, p1, p2);
  MirValue *addPtr = add.get();

  bb->values.push_back(std::move(c1));
  bb->values.push_back(std::move(c2));
  bb->values.push_back(std::move(add));

  bb->terminator = std::make_unique<ReturnTerm>(addPtr);

  fn->blocks.push_back(std::move(bb));
  module.functions.push_back(std::move(fn));

  Codegen cg;
  cg.emitModule(module);
  cg.dumpIR();
}

int main(int argc, char *argv[]) {

  if (argc < 2) {
    cerr << "사용법: hgm <소스파일명> [메게변수]" << endl;
    return 1;
  }

  string filename = argv[1];
  ifstream file(filename);
  if (!file.is_open()) {
    cerr << "파일을 열 수 없습니다: " << filename << endl;
    return 1;
  }

  bool logLexer = false, logParser = false, logBuilder = false,
       logResolver = false, test = false;

  std::unordered_map<std::string, std::function<void()>> longOptions = {
      {"--lexer", [&] { logLexer = true; }},
      {"--parser", [&] { logParser = true; }},
      {"--builder", [&] { logBuilder = true; }},
      {"--resolver", [&] { logResolver = true; }},
      {"--all",
       [&] { logLexer = logParser = logBuilder = logResolver = true; }},
  };

  std::unordered_map<char, std::function<void()>> shortOptions = {
      {'l', [&] { logLexer = true; }},
      {'p', [&] { logParser = true; }},
      {'b', [&] { logBuilder = true; }},
      {'r', [&] { logResolver = true; }},
      {'a', [&] { logLexer = logParser = logBuilder = logResolver = true; }},
      {'t', [&] { test = true; }},
  };

  // ────────────────────────────────
  // 2️⃣ 인자 파싱
  // ────────────────────────────────
  for (int i = 2; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg.rfind("--", 0) == 0) {
      // 긴 옵션 처리
      if (auto it = longOptions.find(arg); it != longOptions.end()) {
        it->second();
      } else {
        std::cerr << "Unknown option: " << arg << std::endl;
      }
    } else if (arg.rfind("-", 0) == 0 && arg.size() > 1) {
      // 단일 문자 옵션 묶음 처리 (-lpn 등)
      for (size_t j = 1; j < arg.size(); ++j) {
        char opt = arg[j];
        if (auto it = shortOptions.find(opt); it != shortOptions.end()) {
          it->second();
        } else {
          std::cerr << "Unknown short option: -" << opt << std::endl;
        }
      }
    } else {
      std::cerr << "Invalid argument format: " << arg << std::endl;
    }
  }

  stringstream buffer;
  buffer << file.rdbuf();
  string source = buffer.str();

  if (test) {
    tester();
    return 0;
  }

  Lexer lexer(source);

  try {
    lexer.lexing();
  } catch (std::runtime_error &e) {
    cout << Color::RED << "error occur while lexing\n"
         << Color::RESET << e.what() << "\n";
    return -1;
  }

  if (logLexer) {
    cout << "===== Lexing result =====" << endl;

    for (const auto &tok : lexer.tokenized) {
      std::cout << "[" << tokenToString(tok.kind) << "] " << tok.text
                << " (line " << tok.line << ", col " << tok.col << ")"
                << std::endl;
    }

    cout << "=========================" << endl;
  }

  Parser parser(lexer.tokenized);

  try {
    parser.parse();
  } catch (std::runtime_error &e) {
    cout << Color::RED << "error occur while parsing\n"
         << Color::RESET << e.what() << "\n";
    return -1;
  }

  if (logParser) {
    ParserDebugger pd;
    cout << "===== parsing result =====" << endl;

    for (auto a : parser.statements)
      a->accept(&pd);

    cout << "=========================" << endl;
  }

  SemanticAnalyzer analyzer(parser.statements);
  try {
    analyzer.build();
  } catch (std::runtime_error &e) {
    cout << Color::RED << "error occur while building\n"
         << Color::RESET << e.what() << "\n";
    return -1;
  }

  if (logBuilder) {
    cout << "===== Building result =====" << endl;

    BuilderDebugger bd(analyzer.symbolTable.getCurrent());
    bd.debug();
    cout << "=========================" << endl;
  }

  try {
    analyzer.link();
  } catch (std::runtime_error &e) {
    cout << Color::RED << "error occur while linking\n"
         << Color::RESET << e.what() << "\n";
    return -1;
  }

  try {
    analyzer.resolve();
  } catch (std::runtime_error &e) {
    cout << Color::RED << "error occur while resolving\n"
         << Color::RESET << e.what() << "\n";
    return -1;
  }

  if (logResolver) {
    cout << "===== Resolving result =====" << endl;

    ResolverDebugger rd(analyzer.symbolTable.getCurrent());
    rd.debug(analyzer.symbolTable.main->rootScope.get());
    rd.debug();
    cout << "=========================" << endl;
  }

  Verifier veifier;

  try {
    for (auto s : parser.statements) {
      s->accept(&veifier);
    }
  } catch (std::runtime_error &e) {
    cout << Color::RED << "error occur while verifing\n"
         << Color::RESET << e.what() << "\n";
    return -1;
  }

  cout << "end compile\n";
  return 0;
}

inline std::string_view tokenToString(TKind kind) {
  auto name = magic_enum::enum_name(kind);
  return name.empty() ? "UNKNOWN" : name;
}
