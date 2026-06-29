#include "hrd/SemanticAnalyzer/ResolvedLit.h"
#include "hrd/SemanticAnalyzer/Resolver.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SourceSpan.h"
#include "hrd/util/Error.h"
#include <vector>

ResolvedLit Resolver::resolveChar(LiteralExpr *expr) {
  auto cp = decodeCharLiteral(expr->span, expr->value);
  TypeSymbol *type;
  if (cp <= 0xFF) {
    type = table->getType("c8");
  } else if (cp <= 0xFFFF) {
    type = table->getType("c16");
  } else
    type = table->getType("c32");

  ResolvedLit r;
  r.type = type;
  r.value = CharPayload(cp);
  return r;
}

ResolvedLit Resolver::resolveString(LiteralExpr *expr) {
  const std::string &s = expr->value;
  uint32_t maxCp = 0;
  std::vector<uint32_t> c;
  size_t i = 0;
  while (i < s.size()) {
    uint32_t cp = decodeOneUtf8CodePoint(expr->span, s, i);
    maxCp = std::max(maxCp, cp);
    c.push_back(cp);
  }

  TypeSymbol *type = nullptr;

  if (maxCp <= 0xFF) {
    type = table->getType("s8");
  } else if (maxCp <= 0xFFFF) {
    type = table->getType("s16");
  } else {
    type = table->getType("s32");
  }

  if (type == nullptr) {
    Error::internal(expr->span, "fail to get string");
  }

  ResolvedLit r;
  r.type = type;
  r.value = StringPayload(c);
  return r;
}

uint32_t Resolver::decodeCharLiteral(const SourceSpan &token, str s) {

  if (s.empty()) {
    Error::diagnostic(token, "empty character literal");
  }

  uint32_t cp = 0;

  if (s[0] != '\\') {
    // 비이스케이프 문자는 UTF-8 문자 하나만 허용한다고 가정
    // 현재 lexer가 이미 한 문자 단위로 잘라줬다면 그냥 decode만 하면 됨.
    // 여기서는 UTF-8 디코딩을 직접 수행.
    const unsigned char b0 = static_cast<unsigned char>(s[0]);

    if (b0 < 0x80) {
      if (s.size() != 1) {
        Error::diagnostic(
            token,
            "character literal must contain exactly one unicode scalar value");
      }
      cp = b0;
    } else if ((b0 & 0xE0) == 0xC0) {
      if (s.size() != 2) {
        Error::diagnostic(
            token,
            "character literal must contain exactly one unicode scalar value");
      }
      const unsigned char b1 = static_cast<unsigned char>(s[1]);
      if ((b1 & 0xC0) != 0x80) {
        Error::diagnostic(token, "invalid UTF-8 sequence in character literal");
      }
      cp = (static_cast<uint32_t>(b0 & 0x1F) << 6) |
           static_cast<uint32_t>(b1 & 0x3F);
      if (cp < 0x80) {
        Error::diagnostic(token,
                          "overlong UTF-8 sequence in character literal");
      }
    } else if ((b0 & 0xF0) == 0xE0) {
      if (s.size() != 3) {
        Error::diagnostic(
            token,
            "character literal must contain exactly one unicode scalar value");
      }
      const unsigned char b1 = static_cast<unsigned char>(s[1]);
      const unsigned char b2 = static_cast<unsigned char>(s[2]);
      if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80) {
        Error::diagnostic(token, "invalid UTF-8 sequence in character literal");
      }
      cp = (static_cast<uint32_t>(b0 & 0x0F) << 12) |
           (static_cast<uint32_t>(b1 & 0x3F) << 6) |
           static_cast<uint32_t>(b2 & 0x3F);
      if (cp < 0x800) {
        Error::diagnostic(token,
                          "overlong UTF-8 sequence in character literal");
      }
    } else if ((b0 & 0xF8) == 0xF0) {
      if (s.size() != 4) {
        Error::diagnostic(
            token,
            "character literal must contain exactly one unicode scalar value");
      }
      const unsigned char b1 = static_cast<unsigned char>(s[1]);
      const unsigned char b2 = static_cast<unsigned char>(s[2]);
      const unsigned char b3 = static_cast<unsigned char>(s[3]);
      if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80) {
        Error::diagnostic(token, "invalid UTF-8 sequence in character literal");
      }
      cp = (static_cast<uint32_t>(b0 & 0x07) << 18) |
           (static_cast<uint32_t>(b1 & 0x3F) << 12) |
           (static_cast<uint32_t>(b2 & 0x3F) << 6) |
           static_cast<uint32_t>(b3 & 0x3F);
      if (cp < 0x10000) {
        Error::diagnostic(token,
                          "overlong UTF-8 sequence in character literal");
      }
    } else {
      Error::diagnostic(token, "invalid UTF-8 sequence in character literal");
    }
  } else {
    // escape sequence
    if (s.size() < 2) {
      Error::diagnostic(token, "invalid escape sequence in character literal");
    }

    switch (s[1]) {
    case '\\':
      if (s.size() != 2)
        Error::diagnostic(token,
                          "invalid escape sequence in character literal");
      cp = U'\\';
      break;
    case '\'':
      if (s.size() != 2)
        Error::diagnostic(token,
                          "invalid escape sequence in character literal");
      cp = U'\'';
      break;
    case '"':
      if (s.size() != 2)
        Error::diagnostic(token,
                          "invalid escape sequence in character literal");
      cp = U'"';
      break;
    case 'n':
      if (s.size() != 2)
        Error::diagnostic(token,
                          "invalid escape sequence in character literal");
      cp = U'\n';
      break;
    case 'r':
      if (s.size() != 2)
        Error::diagnostic(token,
                          "invalid escape sequence in character literal");
      cp = U'\r';
      break;
    case 't':
      if (s.size() != 2)
        Error::diagnostic(token,
                          "invalid escape sequence in character literal");
      cp = U'\t';
      break;
    case '0':
      if (s.size() != 2)
        Error::diagnostic(token,
                          "invalid escape sequence in character literal");
      cp = 0;
      break;

    case 'u':
      if (s.size() != 6) {
        Error::diagnostic(
            token, "unicode escape \\u must contain exactly 4 hex digits");
      }
      cp = parseHex(s, 2, 4);
      break;

    case 'U':
      if (s.size() != 10) {
        Error::diagnostic(
            token, "unicode escape \\U must contain exactly 8 hex digits");
      }
      cp = parseHex(s, 2, 8);
      break;

    default:
      Error::diagnostic(token, "unknown escape sequence in character literal");
    }
  }

  if (!isValidUnicodeScalar(cp)) {
    Error::diagnostic(
        token, "character literal must contain a valid unicode scalar value");
  }

  return cp;
}

uint32_t Resolver::decodeOneUtf8CodePoint(const SourceSpan &token,
                                          const std::string &s, size_t &i) {
  if (i >= s.size()) {
    Error::internal(token, "unexpected end of UTF-8 sequence");
  }

  unsigned char b0 = static_cast<unsigned char>(s[i]);

  uint32_t cp = 0;

  // 1-byte ASCII
  if (b0 < 0x80) {
    cp = b0;
    i += 1;
  }

  // 2-byte sequence
  else if ((b0 & 0xE0) == 0xC0) {
    if (i + 1 >= s.size()) {
      Error::diagnostic(token, "truncated UTF-8 sequence in string literal");
    }

    unsigned char b1 = static_cast<unsigned char>(s[i + 1]);

    if ((b1 & 0xC0) != 0x80) {
      Error::diagnostic(token, "invalid UTF-8 continuation byte");
    }

    uint32_t u0 = static_cast<uint32_t>(b0);
    uint32_t u1 = static_cast<uint32_t>(b1);

    cp = ((u0 & 0x1F) << 6) | (u1 & 0x3F);

    // overlong check
    if (cp < 0x80) {
      Error::diagnostic(token, "overlong UTF-8 encoding");
    }

    i += 2;
  }

  // 3-byte sequence
  else if ((b0 & 0xF0) == 0xE0) {
    if (i + 2 >= s.size()) {
      Error::diagnostic(token, "truncated UTF-8 sequence in string literal");
    }

    unsigned char b1 = static_cast<unsigned char>(s[i + 1]);
    unsigned char b2 = static_cast<unsigned char>(s[i + 2]);

    if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80) {
      Error::diagnostic(token, "invalid UTF-8 continuation byte");
    }
    uint32_t u0 = static_cast<uint32_t>(b0);
    uint32_t u1 = static_cast<uint32_t>(b1);

    cp = ((u0 & 0x0F) << 12) | ((u1 & 0x3F) << 6) | (b2 & 0x3F);

    if (cp < 0x800) {
      Error::diagnostic(token, "overlong UTF-8 encoding");
    }

    i += 3;
  }

  // 4-byte sequence
  else if ((b0 & 0xF8) == 0xF0) {
    if (i + 3 >= s.size()) {
      Error::diagnostic(token, "truncated UTF-8 sequence in string literal");
    }

    unsigned char b1 = static_cast<unsigned char>(s[i + 1]);
    unsigned char b2 = static_cast<unsigned char>(s[i + 2]);
    unsigned char b3 = static_cast<unsigned char>(s[i + 3]);

    if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80) {
      Error::diagnostic(token, "invalid UTF-8 continuation byte");
    }
    uint32_t u0 = static_cast<uint32_t>(b0);
    uint32_t u1 = static_cast<uint32_t>(b1);
    uint32_t u2 = static_cast<uint32_t>(b2);
    uint32_t u3 = static_cast<uint32_t>(b3);
    cp = ((u0 & 0x07) << 18) | ((u1 & 0x3F) << 12) | ((u2 & 0x3F) << 6) |
         (u3 & 0x3F);

    if (cp < 0x10000) {
      Error::diagnostic(token, "overlong UTF-8 encoding");
    }

    i += 4;
  }

  else {
    Error::diagnostic(token, "invalid UTF-8 leading byte");
  }

  // Unicode scalar value validation
  if (cp > 0x10FFFF) {
    Error::diagnostic(token, "invalid Unicode code point");
  }

  if (cp >= 0xD800 && cp <= 0xDFFF) {
    Error::diagnostic(token, "UTF-8 sequence encodes a surrogate code point");
  }

  return cp;
}