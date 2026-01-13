#include "include/Color.h"
#include "include/Lexer.h"
#include "include/Parser.h"
#include "include/Token.h"
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>

using namespace std;

inline string tokenToString(TKind kind);

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
    lexer.lexing();
  } catch (std::runtime_error &e) {
    cout << RED << "error occur while lexing\n" << RESET << e.what() << "\n";
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
      cout << RED << "error occur while parsing\n" << RESET << e.what() <<
      "\n"; return -1;
    }

  //   if (logParser) {
  //     cout << "===== Parsing result =====" << endl;
  //     std::cout << parser.statements.size() << " statements" << std::endl;

  //     PrintVisitor visitor;

  //     for (const auto &stmt : parser.statements)
  //       stmt->accept(&visitor);

  //     cout << "=========================" << endl;
  //   }

  //   SemanticAnalyzer analyzer(parser.program);
  //   try {
  //     analyzer.analyze();
  //   } catch (std::runtime_error &e) {
  //     cout << RED << "error occur while analying\n" << RESET << e.what() <<
  //     "\n"; return -1;
  //   }

  //   if (logAnalyzer) {
  //     cout << "===== finish analzying =====" << endl;
  //   }

  cout << "end compile\n";
  return 0;
}

inline string tokenToString(TKind kind) {
  switch (kind) {
  case TKind::LEFT_PAREN:
    return "LEFT_PAREN";
  case TKind::RIGHT_PAREN:
    return "RIGHT_PAREN";
  case TKind::LEFT_BRACE:
    return "LEFT_BRACE";
  case TKind::RIGHT_BRACE:
    return "RIGHT_BRACE";
  case TKind::LEFT_BRACKET:
    return "LEFT_BRACKET";
  case TKind::RIGHT_BRACKET:
    return "RIGHT_BRACKET";
  case TKind::SEMICOLON:
    return "SEMICOLON";
  case TKind::COLON:
    return "COLON";
  case TKind::COMMA:
    return "COMMA";
  case TKind::DOT:
    return "DOT";
  case TKind::PLUS:
    return "PLUS";
  case TKind::MINUS:
    return "MINUS";
  case TKind::STAR:
    return "STAR";
  case TKind::DOUBLE_STAR:
    return "DOUBLE_STAR";
  case TKind::SLASH:
    return "SLASH";
  case TKind::PERCENT:
    return "PERCENT";
  case TKind::EQUAL:
    return "EQUAL";
  case TKind::PLUS_EQUAL:
    return "PLUS_EQUAL";
  case TKind::MINUS_EQUAL:
    return "MINUS_EQUAL";
  case TKind::STAR_EQUAL:
    return "STAR_EQUAL";
  case TKind::DOUBLE_STAR_EQUAL:
    return "DOUBLE_STAR_EQUAL";
  case TKind::SLASH_EQUAL:
    return "SLASH_EQUAL";
  case TKind::PERCENT_EQUAL:
    return "PERCENT_EQUAL";
  case TKind::DOUBLE_EQUAL:
    return "DOUBLE_EQUAL";
  case TKind::BANG_EQUAL:
    return "BANG_EQUAL";
  case TKind::LESS:
    return "LESS";
  case TKind::GREATER:
    return "GREATER";
  case TKind::LESS_EQUAL:
    return "LESS_EQUAL";
  case TKind::GREATER_EQUAL:
    return "GREATER_EQUAL";
  case TKind::BANG:
    return "BANG";
  case TKind::AND:
    return "AND";
  case TKind::OR:
    return "OR";
  case TKind::QUESTION:
    return "QUESTION";
  case TKind::INT:
    return "INT";
  case TKind::FLOAT:
    return "FLOAT";
  case TKind::FIXED:
    return "FIXED";
  case TKind::CHAR:
    return "CHAR";
  case TKind::STRING:
    return "STRING";
  case TKind::BOOL:
    return "BOOL";
  case TKind::NUL:
    return "NULL";
  case TKind::LIT_INT:
    return "LIT_INT";
  case TKind::LIT_FLOAT:
    return "LIT_FLOAT";
  case TKind::LIT_CHARACTER:
    return "LIT_CHARACTOR";
  case TKind::LIT_STRING:
    return "LIT_STRING";
  case TKind::LIT_BOOL:
    return "LIT_BOOL";
  case TKind::SLASH_STAR:
    return "SLASH_STAR";
  case TKind::STAR_SLASH:
    return "STAR_SLASH";
  case TKind::DOUBLE_SLASH:
    return "DOUBLE_SLASH";
  case TKind::IDENTIFIER:
    return "IDENTIFIER";
  case TKind::END:
    return "END";
  case TKind::IF:
    return "IF";
  case TKind::ELSE:
    return "ELSE";
  case TKind::SWITCH:
    return "SWITCH";
  case TKind::CASE:
    return "CASE";
  case TKind::FOR:
    return "FOR";
  case TKind::WHILE:
    return "WHILE";
  case TKind::BREAK:
    return "BREAK";
  case TKind::CONTINUE:
    return "CONTINUE";
  case TKind::RETURN:
    return "RETURN";
  case TKind::FUNC:
    return "FUNC";
  case TKind::VOID:
    return "VOID";
  case TKind::CARET:
    return "CARET";
  case TKind::BORROW:
    return "BORROW";
  case TKind::CLASS:
    return "CLASS";
  case TKind::STRUCT:
    return "STRUCT";
  case TKind::PUBLIC:
    return "PUBLIC";
  case TKind::PROTECTED:
    return "PROTECTED";
  case TKind::PRIVATE:
    return "PRIVATE";
  case TKind::INTERNAL:
    return "INTERNAL";
  case TKind::IMPL:
    return "IMPL";
  case TKind::TRAIT:
    return "TRAIT";
  case TKind::EXTENDS:
    return "EXTENDS";
  case TKind::EMPTY:
    return "EMPTY";
  case TKind::ENUM:
    return "ENUM";
  case TKind::CONST:
    return "CONST";
  case TKind::ROOT:
  return "ROOT";
  case TKind::NEW:
  return "NEW";
  case TKind::TRY:
  return "TRY";
  case TKind::CATCH:
  return"CATCH";
  case TKind::ONEXIT:
  return "ONEXIT";
  default:
    return "UNKNOWN";
  }
}
