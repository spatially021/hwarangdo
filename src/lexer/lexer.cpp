#include "hrd/Lexer.h"
#include "hrd/AST/TokenStream.h"
#include "hrd/Token.h"
#include "hrd/compiler/CompilerContexts.h"
#include "hrd/util/Error.h"
#include "hrd/util/diagnostic/Diagnostic.h"
#include <cctype>
#include <stdexcept>
#include <sys/types.h>
#include <vector>

using namespace std;

Lexer::Lexer(LexerContext &ctx) : input(ctx.source), engine(ctx.engine) {}

TokenStream Lexer::lexing() {

  line = 1;
  col = 1;
  pos = 0;

  src = input.text;
  path = input.path;

  while (Lexer::peek() != '\0') {
    Token t = Lexer::scan();
    if (t.kind != TKind::EMPTY)
      tokenized.push_back(t);
  }
  return {path, tokenized};
}

char Lexer::get() {
  if (pos >= src.size())
    return '\0';
  char c = src[pos++];
  if (c == '\n') {
    line++;
    col = 1;
  } else {
    col++;
  }
  return c;
}

char Lexer::peek(unsigned int offset) const {
  if (pos + offset >= src.size())
    return '\0';
  return src[pos + offset];
}

void Lexer::skipWS() {
  // skip white space until peek is not white space
  while (isspace(peek()))
    get();
}

bool Lexer::isIdentFirst(char c) {
  // check this character can place first letter of identifer
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool Lexer::isIdentRest(char c) {
  // check this character can place in identifer
  return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9');
}

bool Lexer::isNumber(char c) { return (c >= '0' && c <= '9'); }

Token Lexer::scan() {
  skipWS();
  char c = peek();
  int tempL = line, tempC = col;

  if (!c || c == '\0')
    return {TKind::END, "", {path, tempL, tempC, line, col}};
  // 단일 문자 기호들
  if (c == '(')
    return {
        TKind::LEFT_PAREN, string(1, get()), {path, tempL, tempC, line, col}};
  if (c == ')')
    return {
        TKind::RIGHT_PAREN, string(1, get()), {path, tempL, tempC, line, col}};
  if (c == '{')
    return {
        TKind::LEFT_BRACE, string(1, get()), {path, tempL, tempC, line, col}};
  if (c == '}')
    return {
        TKind::RIGHT_BRACE, string(1, get()), {path, tempL, tempC, line, col}};
  if (c == '[')
    return {
        TKind::LEFT_BRACKET, string(1, get()), {path, tempL, tempC, line, col}};
  if (c == ']')
    return {TKind::RIGHT_BRACKET,
            string(1, get()),
            {path, tempL, tempC, line, col}};
  if (c == ';')
    return {
        TKind::SEMICOLON, string(1, get()), {path, tempL, tempC, line, col}};
  if (c == ':')
    return {TKind::COLON, string(1, get()), {path, tempL, tempC, line, col}};
  if (c == ',')
    return {TKind::COMMA, string(1, get()), {path, tempL, tempC, line, col}};
  if (c == '.') {
    get();
    if (peek() == '.') {
      get();
      return {TKind::DOUBLE_DOT, "..", {path, tempL, tempC, line, col}};
    }
    return {TKind::DOT, ".", {path, tempL, tempC, line, col}};
  }
  if (c == '_')
    return {TKind::UNDERBAR, string(1, get()), {path, tempL, tempC, line, col}};

  if (c == '+') {
    get();
    if (peek() == '=') {
      get();
      return {TKind::PLUS_EQUAL, "+=", {path, tempL, tempC, line, col}};
    }
    return {TKind::PLUS, "+", {path, tempL, tempC, line, col}};
  }
  if (c == '-') {
    get();
    if (peek() == '=') {
      get();
      return {TKind::MINUS_EQUAL, "-=", {path, tempL, tempC, line, col}};
    }
    return {TKind::MINUS, "-", {path, tempL, tempC, line, col}};
  }
  if (c == '*') {
    get();
    if (peek() == '*') {
      get();
      if (peek() == '=') {
        get();
        return {
            TKind::DOUBLE_STAR_EQUAL, "**=", {path, tempL, tempC, line, col}};
      }
      return {TKind::DOUBLE_STAR, "**", {path, tempL, tempC, line, col}};
    }
    if (peek() == '=') {
      get();
      return {TKind::STAR_EQUAL, "*=", {path, tempL, tempC, line, col}};
    }
    return {TKind::STAR, "*", {path, tempL, tempC, line, col}};
  }
  if (c == '/') {
    get();
    if (peek() == '=') {
      get();
      return {TKind::SLASH_EQUAL, "/=", {path, tempL, tempC, line, col}};
    }
    if (peek() == '/') {
      get();
      while (peek() != '\n')
        get();
      return {TKind::EMPTY, "//", {path, tempL, tempC, line, col}};
    }
    if (peek() == '*') {
      while (true) {
        if (peek() == '*') {
          get();
          if (peek() == '/') {
            get();
            break;
          }
        } else {
          get();
        }
      }
      return {TKind::EMPTY, "/*", {path, tempL, tempC, line, col}};
    }

    return {TKind::SLASH, "/", {path, tempL, tempC, line, col}};
  }

  if (c == '%') {
    get();
    if (peek() == '=') {
      get();
      return {TKind::PERCENT_EQUAL, "%=", {path, tempL, tempC, line, col}};
    }
    return {TKind::PERCENT, "%", {path, tempL, tempC, line, col}};
  }

  if (c == '=') {
    get();
    if (peek() == '=') {
      get();
      return {TKind::DOUBLE_EQUAL, "==", {path, tempL, tempC, line, col}};
    }
    if (peek() == '>') {
      get();
      return {TKind::EQAUL_AGNLEBUCKET, "=>", {path, tempL, tempC, line, col}};
    }
    return {TKind::EQUAL, "=", {path, tempL, tempC, line, col}};
  }

  if (c == '!') {
    get();
    if (peek() == '=') {
      get();
      return {TKind::BANG_EQUAL, "!=", {path, tempL, tempC, line, col}};
    }
    return {TKind::BANG, "!", {path, tempL, tempC, line, col}};
  }

  if (c == '<') {
    get();
    if (peek() == '=') {
      get();
      return {TKind::LESS_EQUAL, "<=", {path, tempL, tempC, line, col}};
    }
    if (peek() == '<') {
      get();
      if (peek() == '=') {
        get();
        return {TKind::DOUBLE_ANGLEBUCKET_EQAUL,
                "<<=",
                {path, tempL, tempC, line, col}};
      }
      return {TKind::DOUBLE_ANGLEBUCKET, "<<", {path, tempL, tempC, line, col}};
    }
    return {TKind::LESS, "<", {path, tempL, tempC, line, col}};
  }

  if (c == '>') {
    get();
    if (peek() == '=') {
      get();
      return {TKind::GREATER_EQUAL, ">=", {path, tempL, tempC, line, col}};
    }
    if (peek() == '>') {
      get();
      if (peek() == '=') {
        get();
        return {TKind::DOUBLE_RIGHT_ANGLE_BUCKET_EQUAL,
                ">>=",
                {path, tempL, tempC, line, col}};
      }
      return {TKind::DOUBLE_RIGHT_ANGLE_BUCKET,
              ">>",
              {path, tempL, tempC, line, col}};
    }
    return {TKind::GREATER, ">", {path, tempL, tempC, line, col}};
  }

  if (c == '&') {
    get();
    if (peek() == '&') {
      get();
      return {TKind::AND, "&&", {path, tempL, tempC, line, col}};
    }
    if (peek() == '=') {
      get();
      return {TKind::AMPERSAND_EQAUL, "&=", {path, tempL, tempC, line, col}};
    }
    return {TKind::AMPERSAND, "&", {path, tempL, tempC, line, col}};
  }

  if (c == '~') {
    return {TKind::TILDE, string(1, get()), {path, tempL, tempC, line, col}};
  }

  if (c == '|') {
    get();
    if (peek() == '|') {
      get();
      return {TKind::OR, "||", {path, tempL, tempC, line, col}};
    }
    if (peek() == '=') {
      get();
      return {TKind::PIPE_EQUAL, "|=", {path, tempL, tempC, line, col}};
    }
    return {TKind::PIPE, "|", {path, tempL, tempC, line, col}};
  }

  if (c == '?')
    return {TKind::QUESTION, string(1, get()), {path, tempL, tempC, line, col}};
  if (c == '\0')
    return {TKind::END, string(1, get()), {path, tempL, tempC, line, col}};
  if (c == '^') {
    get(); //
    if (peek() == '=') {
      get();
      return {TKind::CARET_EQUAL, "^=", {path, tempL, tempC, line, col}};
    }
    return {TKind::CARET, "^", {path, tempL, tempC, line, col}};
  }

  if (isIdentFirst(c)) {
    string ident;
    while (isIdentRest(peek())) {
      ident.push_back(get());
    }
    auto it = keyword_map.find(ident);
    if (it != keyword_map.end()) {
      return {it->second,
              ident,
              {path, tempL, tempC, line, col}}; // 키워드인 경우 바로 반환
    }

    if (ident == "true" || ident == "false") {
      return {TKind::LIT_BOOL, ident, {path, tempL, tempC, line, col}};
    }

    return {TKind::IDENTIFIER,
            ident,
            {path, tempL, tempC, line, col}}; // 키워드가 아니면 일반 식별자
  }

  // 문자열
  if (c == '\"') {
    string str;
    get(); // opening "
    while (peek() != '\"' && peek() != '\0') {
      char temp = get();
      str.push_back(temp);
    }
    if (peek() == '\0') {
      get();
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_L001);
      dia.labels = {
          {{path, tempL, tempC, line, col}, "missing closing '\"'", true},
      };
      engine.emit(dia);
      // Error::diagnostic({path, tempL, tempC, line, col},
      //                   "unterminated string literal");
      throw runtime_error("");
    }

    get(); // closing "
    return {TKind::LIT_STRING, str, {path, tempL, tempC, line, col}};
  }

  // 문자 리터럴
  if (c == '\'') {
    string ch;
    get(); // opening '
    char val = get();
    if (val == '\\') { // escape
      char esc = get();
      if (!isEscapeChar(esc)) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_L002);
        dia.labels = {
            {{path, tempL, tempC, line, col}, "invalid escape sequence", true},
        };
        engine.emit(dia);
        throw runtime_error("");
        // Error::diagnostic({path, tempL, tempC, line, col},
        //                   "invalid escape sequence in character literal");
      }
      ch = string("\\") + esc;
    } else {
      ch = string(1, val);
    }
    if (peek() != '\'') {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_L003);
      dia.labels = {
          {{path, tempL, tempC, line, col}, "missing closing '\''", true},
      };
      engine.emit(dia);
      throw runtime_error("");
      // Error::diagnostic({path, tempL, tempC, line, col},
      //                   "unterminated character literal");
    }
    get(); // closing '
    return {TKind::LIT_CHARACTER, ch, {path, tempL, tempC, line, col}};
  }

  // 숫자 리터럴
  if (isNumber(c)) {
    string str;
    bool isReal = false;

    str.push_back(get());
    while (isNumber(peek()))
      str.push_back(get());

    if (peek() == '.') {
      if (peek(1) == '.') {
        return {TKind::LIT_INT, str, {path, tempL, tempC, line, col}};
      }
      str.push_back(get());
      isReal = true;
      while (isNumber(peek()))
        str.push_back(get());
    }

    if (peek() == 'e' || peek() == 'E') {
      str.push_back(get());
      if (peek() == '+' || peek() == '-')
        str.push_back(get());
      while (isNumber(peek()))
        str.push_back(get());
      isReal = true;
    }

    if (isReal) {
      return {TKind::LIT_FLOAT, str, {path, tempL, tempC, line, col}};
    } else {
      return {TKind::LIT_INT, str, {path, tempL, tempC, line, col}};
    }
  }

  // 알 수 없는 토큰
  auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_L004);
  dia.labels = {
      {{path, tempL, tempC, line, col}, "character is not recognized", true},
  };
  engine.emit(dia);
  throw runtime_error("");
  // Error::diagnostic({path, tempL, tempC, line, col},
  //                   "unexpected character '" + string(1, c) + "'");
}

bool Lexer::isEscapeChar(char c) {
  switch (c) {
  case '\'':
  case '\"':
  case '\?':
  case '\\':
  case 'a':
  case 'b':
  case 'f':
  case 'n':
  case 'r':
  case 't':
  case 'v':
    return true;
  default:
    return false;
  }
}
