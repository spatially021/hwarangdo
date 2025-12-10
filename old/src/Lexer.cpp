#include "include/Lexer.h"
#include "include/Token.h"
#include <cctype>
#include <stdexcept>
#include <sys/types.h>

using namespace std;

Lexer::Lexer(const string &s) : src(s) {}

void Lexer::lexe() {
  line = 1;
  col = 1;
  pos = 0;

  while (Lexer::peek() != '\0') {
    Token t = Lexer::scan();
    if (t.kind != TokKind::EMPTY)
      tokenized.push_back(t);
  }
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

char Lexer::peek(int offset) const {
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
  return c == '_' || (c >= 'a' && c < 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9');
}

bool Lexer::isNumber(char c) { return (c >= '0' && c <= '9'); }

Token Lexer::scan() {
  skipWS();
  char c = peek();
  int tempL = line, tempC = col;

  if (!c || c == '\0')
    return {TokKind::END, "", tempL, tempC};

  // 단일 문자 기호들
  if (c == '(')
    return {TokKind::LEFT_PAREN, string(1, get()), tempL, tempC};
  if (c == ')')
    return {TokKind::RIGHT_PAREN, string(1, get()), tempL, tempC};
  if (c == '{')
    return {TokKind::LEFT_BRACE, string(1, get()), tempL, tempC};
  if (c == '}')
    return {TokKind::RIGHT_BRACE, string(1, get()), tempL, tempC};
  if (c == '[')
    return {TokKind::LEFT_BRACKET, string(1, get()), tempL, tempC};
  if (c == ']')
    return {TokKind::RIGHT_BRACKET, string(1, get()), tempL, tempC};
  if (c == ';')
    return {TokKind::SEMICOLON, string(1, get()), tempL, tempC};
  if (c == ':')
    return {TokKind::COLON, string(1, get()), tempL, tempC};
  if (c == ',')
    return {TokKind::COMMA, string(1, get()), tempL, tempC};
  if (c == '.')
    return {TokKind::DOT, string(1, get()), tempL, tempC};
  if (c == '+') {
    get();
    if (peek() == '+') {
      get();
      return {TokKind::DOUBLE_PLUS, "++", tempL, tempC};
    }
    if (peek() == '=') {
      get();
      return {TokKind::PLUS_EQUAL, "+=", tempL, tempC};
    }
    return {TokKind::PLUS, "+", tempL, tempC};
  }
  if (c == '-') {
    get();
    if (peek() == '-') {
      get();
      return {TokKind::DOUBLE_MINUS, "--", tempL, tempC};
    }
    if (peek() == '=') {
      get();
      return {TokKind::MINUS_EQUAL, "-=", tempL, tempC};
    }
    return {TokKind::MINUS, "-", tempL, tempC};
  }
  if (c == '*') {
    get();
    if (peek() == '*') {
      get();
      if (peek() == '=') {
        get();
        return {TokKind::DOUBLE_STAR_EQUAL, "**=", tempL, tempC};
      }
      return {TokKind::DOUBLE_STAR, "**", tempL, tempC};
    }
    if (peek() == '=') {
      get();
      return {TokKind::STAR_EQUAL, "*=", tempL, tempC};
    }
    return {TokKind::STAR, "*", tempL, tempC};
  }
  if (c == '/') {
    get();
    if (peek() == '/') {
      get();
      while (peek() != '\n') {
        get();
      }
      return {TokKind::EMPTY, "//", tempL, tempC};
    }
    if (peek() == '*') {
      get();
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
      return {TokKind::EMPTY, "/*", tempL, tempC};
    }
    if (peek() == '=') {
      get();
      return {TokKind::SLASH_EQUAL, "/=", tempL, tempC};
    }
    return {TokKind::SLASH, "/", tempL, tempC};
  }
  if (c == '%') {
    if (peek() == '=') {
      get();
      return {TokKind::PERCENT_EQUAL, "%=", tempL, tempC};
    }
    return {TokKind::PERCENT, string(1, get()), tempL, tempC};
  }
  // 2글자 연산자
  if (c == '=') {
    get();
    if (peek() == '=') {
      get();
      return {TokKind::EQUAL_EQUAL, "==", tempL, tempC};
    }
    return {TokKind::EQUAL, "=", tempL, tempC};
  }
  if (c == '!') {
    get();
    if (peek() == '=') {
      get();
      return {TokKind::EXCLAIM_EQUAL, "!=", tempL, tempC};
    }
    return {TokKind::EXCLAIM, "!", tempL, tempC};
  }
  if (c == '>') {
    get();
    if (peek() == '=') {
      get();
      return {TokKind::GREATER_EQUAL, ">=", tempL, tempC};
    }
    return {TokKind::GREATER, ">", tempL, tempC};
  }
  if (c == '<') {
    get();
    if (peek() == '=') {
      get();
      return {TokKind::LESS_EQUAL, "<=", tempL, tempC};
    }
    return {TokKind::LESS, "<", tempL, tempC};
  }

  if (c == '?') {
    get();
    return {TokKind::QUESTION, "?", tempL, tempC};
  }

  // 식별자 / 키워드
  if (isIdentFirst(c)) {
    string ident;
    while (isIdentRest(peek())) {
      ident.push_back(get());
    }

    if (ident == "if")
      return {TokKind::IF, ident, tempL, tempC};
    if (ident == "else")
      return {TokKind::ELSE, ident, tempL, tempC};
    if (ident == "for")
      return {TokKind::FOR, ident, tempL, tempC};
    if (ident == "while")
      return {TokKind::WHILE, ident, tempL, tempC};
    if (ident == "return")
      return {TokKind::RETURN, ident, tempL, tempC};
    if (ident == "break")
      return {TokKind::BREAK, ident, tempL, tempC};
    if (ident == "continue")
      return {TokKind::CONTINUE, ident, tempL, tempC};
    if (ident == "default")
      return {TokKind::DEFAULT, ident, tempL, tempC};
    if (ident == "switch")
      return {TokKind::SWITCH, ident, tempL, tempC};
    if (ident == "case")
      return {TokKind::CASE, ident, tempL, tempC};

    // 자료형 키워드
    if (ident == "int")
      return {TokKind::KW_INT, ident, tempL, tempC};
    if (ident == "float")
      return {TokKind::KW_FLOAT, ident, tempL, tempC};
    if (ident == "fixed")
      return {TokKind::KW_FIXED, ident, tempL, tempC};
    if (ident == "boolean")
      return {TokKind::KW_BOOLEAN, ident, tempL, tempC};
    if (ident == "char")
      return {TokKind::KW_CHAR, ident, tempL, tempC};
    if (ident == "string")
      return {TokKind::KW_STRING, ident, tempL, tempC};

    // 불리언 상수 & null
    if (ident == "true" || ident == "false")
      return {TokKind::BOOLEAN, ident, tempL, tempC};
    if (ident == "null")
      return {TokKind::KW_NULL, ident, tempL, tempC};

    // 함수
    if (ident == "func")
      return {TokKind::FUNC, ident, tempL, tempC};
    if (ident == "void")
      return {TokKind::F_VOID, ident, tempL, tempC};

    // 클래스
    if (ident == "class")
      return {TokKind::CLASS, ident, tempL, tempC};
    if(ident=="struct")
      return {TokKind::STRUCT,ident,tempL,tempC};

    return {TokKind::IDENTIFIER, ident, tempL, tempC};
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
    return {TokKind::STRING, str, tempL, tempC};
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
    return {TokKind::CHAR, ch, tempL, tempC};
  }

  // 숫자 리터럴
  if (isNumber(c)) {
    string str;
    bool isReal = false;

    str.push_back(get());
    while (isNumber(peek()))
      str.push_back(get());

    if (peek() == '.') {
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

    try {
      if (isReal) {
        stof(str);
        return {TokKind::FLOAT, str, tempL, tempC};
      } else {
        stoll(str);
        return {TokKind::INTEGER, str, tempL, tempC};
      }
    } catch (const out_of_range &) {
      throw runtime_error("Numeric literal out of range at line " +
                          to_string(line));
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
