#include "hrd/Lexer.h"
#include "hrd/AST/TokenStream.h"
#include "hrd/Recover/LexerRecover.h"
#include "hrd/Token.h"
#include "hrd/compiler/CompilerContexts.h"
#include "hrd/util/Error.h"
#include "hrd/util/diagnostic/Diagnostic.h"
#include <cctype>
#include <sys/types.h>
#include <utility>
#include <vector>

using namespace std;

Lexer::Lexer(LexerContext &ctx)
    : recover(*this), input(ctx.source), engine(ctx.engine) {}

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
  return {path, tokenized, {}};
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
      recover.recover();
    }

    get(); // closing "
    return {TKind::LIT_STRING, str, {path, tempL, tempC, line, col}};
  }

  // 문자 리터럴
  if (c == '\'') {
    string ch;
    get(); // opening '
    char val = peek();
    if (val == '\\') { // escape
      get();           // \처리
      char esc = get();
      if (!isEscapeChar(esc)) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_L002);
        dia.labels = {
            {{path, tempL, tempC, line, col}, "invalid escape sequence", true},
        };
        engine.emit(dia);
        recover.recover();
      }
      ch = string("\\") + esc;
    } else {
      auto scalar = consumeUtf8Scalar();
      ch = std::move(scalar.bytes);
    }
    if (peek() != '\'') {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_L003);
      dia.labels = {
          {{path, tempL, tempC, line, col}, "missing closing '\''", true},
      };
      engine.emit(dia);
      recover.recover();
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
      if (!isNumber(peek())) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_L006);
        dia.labels = {{{path, tempL, tempC, line, col},
                       "expected digit after decimal point",
                       true}};
        engine.emit(dia);
        recover.recover();
      }

      while (isNumber(peek()))
        str.push_back(get());
    }

    if (peek() == 'e' || peek() == 'E') {
      str.push_back(get());

      if (peek() == '+' || peek() == '-')
        str.push_back(get());

      if (!isNumber(peek())) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_L006);
        dia.labels = {{{path, tempL, tempC, line, col},
                       "expected digit in exponent",
                       true}};
        engine.emit(dia);
        recover.recover();
      }

      while (isNumber(peek()))
        str.push_back(get());

      isReal = true;
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
  recover.recover();
  return {};
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

Utf8Scalar Lexer::consumeUtf8Scalar() {
  const auto startLine = line;
  const auto startColumn = col;

  if (peek() == '\0') {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_L005);
    dia.labels = {
        {{path, startLine, startColumn, line, col},
         "expected Unicode scalar value",
         true},
    };
    engine.emit(dia);
    recover.recover();
  }

  const auto first = static_cast<unsigned char>(peek());

  std::size_t length = 0;
  char32_t value = 0;
  char32_t minimumValue = 0;

  if (first <= 0x7F) {
    length = 1;
    value = first;
    minimumValue = 0;
  } else if ((first & 0xE0) == 0xC0) {
    length = 2;
    value = first & 0x1F;
    minimumValue = 0x80;
  } else if ((first & 0xF0) == 0xE0) {
    length = 3;
    value = first & 0x0F;
    minimumValue = 0x800;
  } else if ((first & 0xF8) == 0xF0) {
    length = 4;
    value = first & 0x07;
    minimumValue = 0x10000;
  } else {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_L005);
    dia.labels = {
        {{path, startLine, startColumn, line, col},
         "invalid UTF-8 leading byte",
         true},
    };
    engine.emit(dia);
    recover.recover();
  }
  std::string bytes;
  bytes.reserve(length);
  bytes.push_back(get());

  for (std::size_t ii = 1; ii < length; ++ii) {
    if (peek() == '\0') {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_L005);
      dia.labels = {
          {{path, startLine, startColumn, line, col},
           "incomplete UTF-8 sequence",
           true},
      };
      engine.emit(dia);
      recover.recover();
    }

    const auto byte = static_cast<unsigned char>(peek());

    if ((byte & 0xC0) != 0x80) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_L005);
      dia.labels = {
          {{path, startLine, startColumn, line, col},
           "expected UTF-8 continuation byte",
           true},
      };
      engine.emit(dia);
      recover.recover();
    }

    bytes.push_back(get());
    value = (value << 6) | (byte & 0x3F);
  }

  if (value < minimumValue) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_L005);
    dia.labels = {
        {{path, startLine, startColumn, line, col},
         "overlong UTF-8 encoding",
         true},
    };
    engine.emit(dia);
    recover.recover();
  }

  if (value >= 0xD800 && value <= 0xDFFF) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_L005);
    dia.labels = {
        {{path, startLine, startColumn, line, col},
         "surrogate code point is not a Unicode scalar value",
         true},
    };
    engine.emit(dia);
    recover.recover();
  }

  if (value > 0x10FFFF) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_L005);
    dia.labels = {
        {{path, startLine, startColumn, line, col},
         "code point is outside the Unicode range",
         true},
    };
    engine.emit(dia);
    recover.recover();
  }

  return {
      value,
      std::move(bytes),
  };
}