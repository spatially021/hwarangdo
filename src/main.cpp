#include "AST/Program.h"
#include "AST/TokenStream.h"
#include "Color.h"
#include "Debugger/BuilderDebugger.h"
#include "Debugger/ParserDebugger.h"
#include "Debugger/ResolverDebugger.h"
#include "IR/HIR/HIRBuilder.h"
#include "IR/HIR/HIRLinker.h"
#include "IR/HIR/HIRProgram.h"
#include "Inputs.h"
#include "Lexer.h"
#include "Parser.h"
#include "SemanticAnalyzer.h"
#include "SemanticAnalyzer/Verifier.h"
#include "Token.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <magic_enum/magic_enum.hpp>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace std;
namespace fs = std::filesystem;

inline string_view tokenToString(TKind kind);

ProjectInput projectInput;

bool logLexer = false, logParser = false, logBuilder = false,
     logResolver = false, test = false;

void tester() {}

// --------------------------------------------------
// 유틸
// --------------------------------------------------
static bool readTextFile(const fs::path &path, std::string &out) {
  std::ifstream file(path, std::ios::in | std::ios::binary);
  if (!file.is_open()) {
    return false;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  out = buffer.str();
  return true;
}

static bool isTomlFile(const fs::path &path) {
  return path.has_extension() && path.extension() == ".toml";
}

static bool isHgmFile(const fs::path &path) {
  return path.has_extension() && path.extension() == ".hrd";
}

// --------------------------------------------------
// 프로젝트 루트 아래에서 toml 찾기
// 현재는 "프로젝트 디렉토리 바로 아래"만 찾음
// --------------------------------------------------
static bool findProjectFile(const fs::path &projectRoot,
                            fs::path &outTomlPath) {
  if (!fs::exists(projectRoot) || !fs::is_directory(projectRoot)) {
    std::cerr << "프로젝트 디렉토리가 존재하지 않거나 디렉토리가 아닙니다: "
              << projectRoot.string() << std::endl;
    return false;
  }

  std::vector<fs::path> tomlCandidates;

  for (const auto &entry : fs::directory_iterator(projectRoot)) {
    if (!entry.is_regular_file()) {
      continue;
    }

    const fs::path &path = entry.path();
    if (isTomlFile(path)) {
      tomlCandidates.push_back(path);
    }
  }

  if (tomlCandidates.empty()) {
    std::cerr << "프로젝트 파일(.toml)을 찾을 수 없습니다: "
              << projectRoot.string() << std::endl;
    return false;
  }

  if (tomlCandidates.size() > 1) {
    std::cerr << "프로젝트 파일(.toml)이 둘 이상 발견되었습니다:\n";
    for (const auto &p : tomlCandidates) {
      std::cerr << "  - " << p.string() << "\n";
    }
    return false;
  }

  outTomlPath = tomlCandidates.front();
  return true;
}

// --------------------------------------------------
// src 디렉토리 찾기
// 현재 정책: 프로젝트 루트 바로 아래 src/
// --------------------------------------------------
static bool findSrcDirectory(const fs::path &projectRoot,
                             fs::path &outSrcPath) {
  fs::path srcPath = projectRoot / "src";

  if (!fs::exists(srcPath) || !fs::is_directory(srcPath)) {
    std::cerr << "src 디렉토리를 찾을 수 없습니다: " << srcPath.string()
              << std::endl;
    return false;
  }

  outSrcPath = srcPath;
  return true;
}

// --------------------------------------------------
// src 아래 .hgm 파일 재귀 수집
// --------------------------------------------------
static bool collectHgmSources(const fs::path &srcRoot,
                              std::vector<InputSource> &outSources) {
  std::vector<fs::path> hgmPaths;

  for (const auto &entry : fs::recursive_directory_iterator(srcRoot)) {
    if (!entry.is_regular_file()) {
      continue;
    }

    const fs::path &path = entry.path();
    if (isHgmFile(path)) {
      hgmPaths.push_back(path);
    }
  }

  if (hgmPaths.empty()) {
    std::cerr << "src 아래에 .hgm 파일이 없습니다: " << srcRoot.string()
              << std::endl;
    return false;
  }

  std::sort(hgmPaths.begin(), hgmPaths.end());

  outSources.clear();
  outSources.reserve(hgmPaths.size());

  for (const auto &path : hgmPaths) {
    InputSource src;
    src.path = path.lexically_normal().string();

    if (!readTextFile(path, src.text)) {
      std::cerr << "소스 파일을 읽을 수 없습니다: " << path.string()
                << std::endl;
      return false;
    }

    outSources.push_back(std::move(src));
  }

  return true;
}

// --------------------------------------------------
// 옵션 파싱
// --------------------------------------------------
static bool parseOptions(int argc, char *argv[]) {
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

  for (int i = 2; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg.rfind("--", 0) == 0) {
      auto it = longOptions.find(arg);
      if (it == longOptions.end()) {
        std::cerr << "Unknown option: " << arg << std::endl;
        return false;
      }
      it->second();
      continue;
    }

    if (arg.rfind("-", 0) == 0 && arg.size() > 1) {
      for (size_t j = 1; j < arg.size(); ++j) {
        char opt = arg[j];
        auto it = shortOptions.find(opt);
        if (it == shortOptions.end()) {
          std::cerr << "Unknown short option: -" << opt << std::endl;
          return false;
        }
        it->second();
      }
      continue;
    }

    std::cerr << "Invalid argument format: " << arg << std::endl;
    return false;
  }

  return true;
}

// --------------------------------------------------
// 입력
// argv[1]은 "프로젝트 디렉토리"
// 내부에서 toml 파일과 src를 찾음
// --------------------------------------------------
bool input(int argc, char *argv[]) {
  if (argc < 2) {
    cerr << "사용법: hgm <프로젝트디렉토리> [매개변수]" << endl;
    return false;
  }

  if (!parseOptions(argc, argv)) {
    return false;
  }

  fs::path projectRoot = fs::path(argv[1]).lexically_normal();

  fs::path tomlPath;
  if (!findProjectFile(projectRoot, tomlPath)) {
    return false;
  }

  fs::path srcPath;
  if (!findSrcDirectory(projectRoot, srcPath)) {
    return false;
  }

  std::vector<InputSource> collectedSources;
  if (!collectHgmSources(srcPath, collectedSources)) {
    return false;
  }

  projectInput.rootPath = projectRoot.string();
  projectInput.projectFilePath = tomlPath.string();
  projectInput.srcPath = srcPath.string();
  projectInput.sources = std::move(collectedSources);

  return true;
}

int main(int argc, char *argv[]) {
  if (!input(argc, argv)) {
    return 1;
  }

  if (projectInput.sources.empty()) {
    std::cerr << "로드된 소스가 없습니다." << std::endl;
    return 1;
  }

  // 임시:
  // 아직 전체 multi-file 파이프라인으로 안 올렸으므로 첫 파일만 사용

  std::cout << "project root : " << projectInput.rootPath << "\n";
  std::cout << "project file : " << projectInput.projectFilePath << "\n";
  std::cout << "src root     : " << projectInput.srcPath << "\n";
  std::cout << "source count : " << projectInput.sources.size() << "\n";
  std::vector<TokenStream> tokenStreams;

  for (auto const &i : projectInput.sources) {
    Lexer lexer(i);
    try {
      tokenStreams.push_back(lexer.lexing());
    } catch (std::runtime_error &e) {
      cout << Color::RED << "error occur while lexing\n"
           << Color::RESET << e.what() << "\n";
      return -1;
    }
  }

  if (logLexer) {
    cout << "===== Lexing result =====" << endl;
    for (auto const &stream : tokenStreams) {

      for (const auto &tok : stream.tokens) {
        std::cout << "[" << tokenToString(tok.kind) << "] " << tok.text
                  << " (line " << tok.line << ", col " << tok.col << ")"
                  << std::endl;
      }
    }
    cout << "=========================" << endl;
  }
  unique_ptr<Program> program = make_unique<Program>();

  for (auto s : tokenStreams) {
    Parser parser(s);
    try {
      program->sources.push_back(
          make_shared<SourceFile>(s.path, parser.parse()));

    } catch (std::runtime_error &e) {
      cout << Color::RED << "error occur while parsing\n"
           << Color::RESET << e.what() << "\n";
      return -1;
    }
  }

  if (logParser) {
    ParserDebugger pd;
    cout << "===== parsing result =====" << endl;
    for (auto &s : program->sources) {
      for (auto &a : s->decls) {
        a->accept(&pd);
      }
    }
    cout << "=========================" << endl;
  }

  SemanticAnalyzer analyzer(program.get());

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
    bd.debug(analyzer.symbolTable.rootScope.get());
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
    rd.debug(analyzer.symbolTable.rootScope.get());
    rd.debug(analyzer.symbolTable.main->rootScope.get());
    rd.debug();
    cout << "=========================" << endl;
  }

  Verifier verifier;

  try {
    for (auto &s : program->sources) {
      for (auto &a : s->decls) {
        a->accept(&verifier);
      }
    }
  } catch (std::runtime_error &e) {
    cout << Color::RED << "error occur while verifying\n"
         << Color::RESET << e.what() << "\n";
    return -1;
  }

  unique_ptr<HIRProgram> hirProgram =
      make_unique<HIRProgram>(&analyzer.symbolTable);

  try {
    vector<HIRSource *> sources;
    for (auto &s : program->sources) {
      HIRLinker linker(hirProgram.get(), s.get());
      hirProgram->sources.push_back(linker.link());
    }
    hirProgram->linkRoot();
    for (auto &s : hirProgram->sources) {
      HIRBuilder builder(&analyzer.symbolTable, hirProgram.get(), s.get());
      builder.build();
    }

  } catch (std::runtime_error &e) {
    cout << Color::RED << "error occur while hir building\n"
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