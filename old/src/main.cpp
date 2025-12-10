#include "Headers.h"
#include "include/AST/ASTPrinter.h"
#include "include/AST/Decl.h"
#include "include/Parser.h"
#include "include/SemanticAnalyzer.h"
#include <functional>
#include <ostream>

using namespace std;

int main(int argc, char *argv[]) {
  if (argc < 2) {
    cerr << "사용법: lexer_test <소스파일명> [메게변수]" << endl;
    return 1;
  }

  string filename = argv[1];
  ifstream file(filename);
  if (!file.is_open()) {
    cerr << "파일을 열 수 없습니다: " << filename << endl;
    return 1;
  }

  bool logLexer = false, logParser = false, logAnalyzer = false;

  std::unordered_map<std::string, std::function<void()>> longOptions = {
      {"--lexer", [&] { logLexer = true; }},
      {"--parser", [&] { logParser = true; }},
      {"--analyzer", [&] { logAnalyzer = true; }},
      {"--all", [&] { logLexer = logParser = logAnalyzer = true; }},
  };

  std::unordered_map<char, std::function<void()>> shortOptions = {
      {'l', [&] { logLexer = true; }},
      {'p', [&] { logParser = true; }},
      {'n', [&] { logAnalyzer = true; }},
      {'a', [&] { logLexer = logParser = logAnalyzer = true; }},
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

  Lexer lexer(source);
  try {
    lexer.lexe();
  } catch (std::runtime_error &e) {
    cout << RED << "error occur while lexing\n" << RESET << e.what() << "\n";
    return -1;
  }

  if (logLexer) {
    cout << "===== Lexing result =====" << endl;

    for (const auto &tok : lexer.tokenized) {
      std::cout << "[" << tokKindToString(tok.kind) << "] " << tok.text
                << " (line " << tok.line << ", col " << tok.col << ")"
                << std::endl;
    }

    cout << "=========================" << endl;
  }

  Parser parser(lexer.tokenized);

  try {
    parser.parse();
  } catch (std::runtime_error &e) {
    cout << RED << "error occur while parsing\n" << RESET << e.what() << "\n";
    return -1;
  }

  if (logParser) {
    cout << "===== Parsing result =====" << endl;
    std::cout << parser.statements.size() << " statements" << std::endl;

    PrintVisitor visitor;

    for (const auto &stmt : parser.statements)
      stmt->accept(&visitor);

    cout << "=========================" << endl;
  }

  SemanticAnalyzer analyzer(parser.program);
  try {
    analyzer.analyze();
  } catch (std::runtime_error &e) {
    cout << RED << "error occur while analying\n" << RESET << e.what() << "\n";
    return -1;
  }

  if (logAnalyzer) {
    cout << "===== finish analzying =====" << endl;
  }

  cout << "end compile\n";
  return 0;
}
/*
#pragma once
#include "SymbolTable.h"
#include "AST/Decl.h"
#include "AST/Stmt.h"
#include "AST/Expr.h"
#include "AST/TypeNode.h"
#include "AST/Program.h"
#include <stdexcept>
#include <iostream>

class SemanticAnalyzer : public ASTVisitor {
    SymbolTable symbols;
    std::string currentReturnType;

public:
    void analyze(Program* root) {
        root->accept(this);
    }

    // ────────────────────────────────
    // 주요 처리 (실제 구현할 부분)
    // ────────────────────────────────
    void visit(FuncDecl* decl) override {
        SymbolInfo info;
        info.name = decl->name;
        info.type = decl->returnType;
        for (auto& p : decl->params)
            info.paramTypes.push_back(p->type);

        if (!symbols.define(FUNC, info))
            throw std::runtime_error("Function redefinition: " + decl->name);

        symbols.enterScope();
        currentReturnType = decl->returnType;

        for (auto& p : decl->params) {
            SymbolInfo param;
            param.name = p->name;
            param.type = p->type;
            if (!symbols.define(VAR, param))
                throw std::runtime_error("Duplicate parameter: " + p->name);
        }

        if (decl->body)
            decl->body->accept(this);

        symbols.exitScope();
    }

    void visit(VarDecl* decl, bool isInFor) override {
        SymbolInfo info;
        info.name = decl->name;
        info.type = decl->type;

        if (!symbols.define(VAR, info))
            throw std::runtime_error("Duplicate variable: " + decl->name);
    }

    // ────────────────────────────────
    // 나머지는 일단 빈 정의 (stub)
    // ────────────────────────────────
};


*/