#include "Lexer.h"
#include "AST/TokenStream.h"
#include "Token.h"
#include <cctype>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <vector>

using namespace std;

Lexer::Lexer(InputSource in) : input(in) {}

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
  return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
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
    return {TKind::END, "", path, tempL, tempC};
  // 단일 문자 기호들
  if (c == '(')
    return {TKind::LEFT_PAREN, string(1, get()), path, tempL, tempC};
  if (c == ')')
    return {TKind::RIGHT_PAREN, string(1, get()), path, tempL, tempC};
  if (c == '{')
    return {TKind::LEFT_BRACE, string(1, get()), path, tempL, tempC};
  if (c == '}')
    return {TKind::RIGHT_BRACE, string(1, get()), path, tempL, tempC};
  if (c == '[')
    return {TKind::LEFT_BRACKET, string(1, get()), path, tempL, tempC};
  if (c == ']')
    return {TKind::RIGHT_BRACKET, string(1, get()), path, tempL, tempC};
  if (c == ';')
    return {TKind::SEMICOLON, string(1, get()), path, tempL, tempC};
  if (c == ':')
    return {TKind::COLON, string(1, get()), path, tempL, tempC};
  if (c == ',')
    return {TKind::COMMA, string(1, get()), path, tempL, tempC};
  if (c == '.') {
    get();
    if (peek() == '.') {
      get();
      return {TKind::DOUBLE_DOT, "..", path, tempL, tempC};
    }
    return {TKind::DOT, ".", path, tempL, tempC};
  }
  if (c == '_')
    return {TKind::UNDERBAR, string(1, get()), path, tempL, tempC};

  if (c == '+') {
    get();
    if (peek() == '=') {
      get();
      return {TKind::PLUS_EQUAL, "+=", path, tempL, tempC};
    }
    return {TKind::PLUS, "+", path, tempL, tempC};
  }
  if (c == '-') {
    get();
    if (peek() == '=') {
      get();
      return {TKind::MINUS_EQUAL, "-=", path, tempL, tempC};
    }
    return {TKind::MINUS, "-", path, tempL, tempC};
  }
  if (c == '*') {
    get();
    if (peek() == '*') {
      get();
      if (peek() == '=') {
        get();
        return {TKind::DOUBLE_STAR_EQUAL, "**=", path, tempL, tempC};
      }
      return {TKind::DOUBLE_STAR, "**", path, tempL, tempC};
    }
    if (peek() == '=') {
      get();
      return {TKind::STAR_EQUAL, "*=", path, tempL, tempC};
    }
    return {TKind::STAR, "*", path, tempL, tempC};
  }
  if (c == '/') {
    get();
    if (peek() == '=') {
      get();
      return {TKind::SLASH_EQUAL, "/=", path, tempL, tempC};
    }
    if (peek() == '/') {
      get();
      while (peek() != '\n')
        get();
      return {TKind::EMPTY, "//", path, tempL, tempC};
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
      return {TKind::EMPTY, "/*", path, tempL, tempC};
    }

    return {TKind::SLASH, "/", path, tempL, tempC};
  }

  if (c == '%') {
    get();
    if (peek() == '=') {
      get();
      return {TKind::PERCENT_EQUAL, "%=", path, tempL, tempC};
    }
    return {TKind::PERCENT, "%", path, tempL, tempC};
  }

  if (c == '=') {
    get();
    if (peek() == '=') {
      get();
      return {TKind::DOUBLE_EQUAL, "==", path, tempL, tempC};
    }
    if (peek() == '>') {
      get();
      return {TKind::EQAUL_AGNLEBUCKET, "=>", path, tempL, tempC};
    }
    return {TKind::EQUAL, "=", path, tempL, tempC};
  }

  if (c == '!') {
    get();
    if (peek() == '=') {
      get();
      return {TKind::BANG_EQUAL, "!=", path, tempL, tempC};
    }
    return {TKind::BANG, "!", path, tempL, tempC};
  }

  if (c == '<') {
    get();
    if (peek() == '=') {
      get();
      return {TKind::LESS_EQUAL, "<=", path, tempL, tempC};
    }
    if (peek() == '<') {
      get();
      if (peek() == '=') {
        get();
        return {TKind::DOUBLE_ANGLEBUCKET_EQAUL, "<<=", path, tempL, tempC};
      }
      return {TKind::DOUBLE_ANGLEBUCKET, "<<", path, tempL, tempC};
    }
    return {TKind::LESS, "<", path, tempL, tempC};
  }

  if (c == '>') {
    get();
    if (peek() == '=') {
      get();
      return {TKind::GREATER_EQUAL, ">=", path, tempL, tempC};
    }
    if (peek() == '>') {
      get();
      if (peek() == '=') {
        get();
        return {TKind::DOUBLE_RIGHT_ANGLE_BUCKET_EQUAL, ">>=", path, tempL,
                tempC};
      }
      return {TKind::DOUBLE_RIGHT_ANGLE_BUCKET, ">>", path, tempL, tempC};
    }
    return {TKind::GREATER, ">", path, tempL, tempC};
  }

  if (c == '&') {
    get();
    if (peek() == '&') {
      get();
      return {TKind::AND, "&&", path, tempL, tempC};
    }
    if (peek() == '=') {
      get();
      return {TKind::AMPERSAND_EQAUL, "&=", path, tempL, tempC};
    }
    return {TKind::AMPERSAND, "&", path, tempL, tempC};
  }

  if (c == '~') {
    return {TKind::TILDE, string(1, get()), path, tempL, tempC};
  }

  if (c == '|') {
    get();
    if (peek() == '|') {
      get();
      return {TKind::OR, "||", path, tempL, tempC};
    }
    if (peek() == '=') {
      get();
      return {TKind::PIPE_EQUAL, "|=", path, tempL, tempC};
    }
    return {TKind::PIPE, "|", path, tempL, tempC};
  }

  if (c == '?')
    return {TKind::QUESTION, string(1, get()), path, tempL, tempC};
  if (c == '\0')
    return {TKind::END, string(1, get()), path, tempL, tempC};
  if (c == '^')
    return {TKind::CARET, string(1, get()), path, tempL, tempC};

  if (isIdentFirst(c)) {
    string ident;
    while (isIdentRest(peek())) {
      ident.push_back(get());
    }
    auto it = keyword_map.find(ident);
    if (it != keyword_map.end()) {
      return {it->second, ident, path, tempL, tempC}; // 키워드인 경우 바로 반환
    }

    if (ident == "true" || ident == "false") {
      return {TKind::LIT_BOOL, ident, path, tempL, tempC};
    }

    return {TKind::IDENTIFIER, ident, path, tempL,
            tempC}; // 키워드가 아니면 일반 식별자
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
      throw runtime_error("Unterminated string literal");
    }
    get(); // closing "
    return {TKind::LIT_STRING, str, path, tempL, tempC};
  }

  // 문자 리터럴
  if (c == '\'') {
    string ch;
    get(); // opening '
    char val = get();
    if (val == '\\') { // escape
      char esc = get();
      if (!isEscapeChar(esc)) {
        throw runtime_error("Invalid escape sequence in char literal");
      }
      ch = string("\\") + esc;
    } else {
      ch = string(1, val);
    }
    if (peek() != '\'') {
      throw runtime_error("Unterminated character literal");
    }
    get(); // closing '
    return {TKind::LIT_CHARACTER, ch, path, tempL, tempC};
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
        return {TKind::LIT_INT, str, path, tempL, tempC};
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
      return {TKind::LIT_FLOAT, str, path, tempL, tempC};
    } else {
      return {TKind::LIT_INT, str, path, tempL, tempC};
    }
  }

  // 알 수 없는 토큰
  throw runtime_error("Unexpected character '" + string(1, c) + "' at line " +
                      to_string(line) + ", col " + to_string(col));
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
