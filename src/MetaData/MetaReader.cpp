#include "hrd/MetaData/MetaReader.h"
#include "hrd/util/Error.h"

#include <algorithm>
#include <cctype>
#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/APInt.h>
#include <llvm/Support/Error.h>
#include <sstream>
#include <stdexcept>

// ============================================================
// Public
// ============================================================

MetaReadResult MetaReader::read(std::istream &in,
                                const std::filesystem::path &inputPath) {
  path = inputPath;
  current = 0;
  tokens.clear();

  std::ostringstream buffer;
  buffer << in.rdbuf();

  tokenize(buffer.str());

  return parse();
}

// ============================================================
// Lexer
// ============================================================

void MetaReader::tokenize(std::string_view source) {
  std::size_t i = 0;
  std::size_t line = 1;
  std::size_t column = 1;

  auto push = [&](TokenKind kind, std::string text, std::size_t tokenLine,
                  std::size_t tokenColumn) {
    tokens.push_back({
        kind,
        std::move(text),
        tokenLine,
        tokenColumn,
    });
  };

  auto advanceChar = [&]() -> char {
    const char c = source[i++];

    if (c == '\n') {
      ++line;
      column = 1;
    } else {
      ++column;
    }

    return c;
  };

  while (i < source.size()) {
    const char c = source[i];

    // ------------------------------------------------------------
    // whitespace
    // ------------------------------------------------------------

    if (std::isspace(static_cast<unsigned char>(c))) {
      advanceChar();
      continue;
    }

    const std::size_t tokenLine = line;
    const std::size_t tokenColumn = column;

    // ------------------------------------------------------------
    // identifier
    // ------------------------------------------------------------

    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
      const std::size_t start = i;

      while (i < source.size()) {
        const char ch = source[i];

        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') {
          break;
        }

        advanceChar();
      }

      push(TokenKind::Identifier, std::string(source.substr(start, i - start)),
           tokenLine, tokenColumn);

      continue;
    }

    // ------------------------------------------------------------
    // number
    // ------------------------------------------------------------

    if (std::isdigit(static_cast<unsigned char>(c)) ||
        (c == '-' && i + 1 < source.size() &&
         std::isdigit(static_cast<unsigned char>(source[i + 1])))) {

      const std::size_t start = i;

      if (source[i] == '-') {
        advanceChar();
      }

      while (i < source.size() &&
             std::isdigit(static_cast<unsigned char>(source[i]))) {
        advanceChar();
      }

      bool isFloat = false;

      if (i < source.size() && source[i] == '.') {
        isFloat = true;
        advanceChar();

        while (i < source.size() &&
               std::isdigit(static_cast<unsigned char>(source[i]))) {
          advanceChar();
        }
      }

      if (i < source.size() && (source[i] == 'e' || source[i] == 'E')) {
        isFloat = true;
        advanceChar();

        if (i < source.size() && (source[i] == '+' || source[i] == '-')) {
          advanceChar();
        }

        while (i < source.size() &&
               std::isdigit(static_cast<unsigned char>(source[i]))) {
          advanceChar();
        }
      }

      push(isFloat ? TokenKind::Float : TokenKind::Integer,
           std::string(source.substr(start, i - start)), tokenLine,
           tokenColumn);

      continue;
    }

    // ------------------------------------------------------------
    // string
    // ------------------------------------------------------------

    if (c == '"') {
      advanceChar();

      std::string value;

      bool closed = false;

      while (i < source.size()) {
        const char ch = advanceChar();

        if (ch == '"') {
          closed = true;
          break;
        }

        if (ch == '\\') {
          value.push_back('\\');

          if (i >= source.size()) {
            break;
          }

          const char escaped = advanceChar();
          value.push_back(escaped);

          if (escaped == 'u' && i < source.size() && source[i] == '{') {
            value.push_back(advanceChar());

            while (i < source.size()) {
              const char hex = advanceChar();

              value.push_back(hex);

              if (hex == '}') {
                break;
              }
            }
          }

          continue;
        }

        value.push_back(ch);
      }

      if (!closed) {
        Error::meta(path.string(), tokenLine, tokenColumn,
                    "unterminated metadata string literal");
      }

      push(TokenKind::String, std::move(value), tokenLine, tokenColumn);

      continue;
    }

    // ------------------------------------------------------------
    // char
    // ------------------------------------------------------------

    if (c == '\'') {
      advanceChar();

      std::string value;

      bool closed = false;

      while (i < source.size()) {
        const char ch = advanceChar();

        if (ch == '\'') {
          closed = true;
          break;
        }

        if (ch == '\\') {
          value.push_back('\\');

          if (i >= source.size()) {
            break;
          }

          const char escaped = advanceChar();
          value.push_back(escaped);

          if (escaped == 'u' && i < source.size() && source[i] == '{') {
            value.push_back(advanceChar());

            while (i < source.size()) {
              const char hex = advanceChar();

              value.push_back(hex);

              if (hex == '}') {
                break;
              }
            }
          }

          continue;
        }

        value.push_back(ch);
      }

      if (!closed) {
        Error::meta(path.string(), tokenLine, tokenColumn,
                    "unterminated metadata char literal");
      }

      push(TokenKind::Char, std::move(value), tokenLine, tokenColumn);

      continue;
    }

    // ------------------------------------------------------------
    // punctuation
    // ------------------------------------------------------------

    switch (c) {
    case '{':
      advanceChar();
      push(TokenKind::LBrace, "{", tokenLine, tokenColumn);
      continue;

    case '}':
      advanceChar();
      push(TokenKind::RBrace, "}", tokenLine, tokenColumn);
      continue;

    case '(':
      advanceChar();
      push(TokenKind::LParen, "(", tokenLine, tokenColumn);
      continue;

    case ')':
      advanceChar();
      push(TokenKind::RParen, ")", tokenLine, tokenColumn);
      continue;

    case '[':
      advanceChar();
      push(TokenKind::LBracket, "[", tokenLine, tokenColumn);
      continue;

    case ']':
      advanceChar();
      push(TokenKind::RBracket, "]", tokenLine, tokenColumn);
      continue;

    case '<':
      advanceChar();
      push(TokenKind::LAngle, "<", tokenLine, tokenColumn);
      continue;

    case '>':
      advanceChar();
      push(TokenKind::RAngle, ">", tokenLine, tokenColumn);
      continue;

    case ',':
      advanceChar();
      push(TokenKind::Comma, ",", tokenLine, tokenColumn);
      continue;

    case ';':
      advanceChar();
      push(TokenKind::Semicolon, ";", tokenLine, tokenColumn);
      continue;

    case '=':
      advanceChar();
      push(TokenKind::Equal, "=", tokenLine, tokenColumn);
      continue;

    case ':':
      advanceChar();

      if (i < source.size() && source[i] == ':') {
        advanceChar();

        push(TokenKind::DoubleColon, "::", tokenLine, tokenColumn);
      } else {
        push(TokenKind::Colon, ":", tokenLine, tokenColumn);
      }

      continue;

    case '-':
      advanceChar();

      if (i < source.size() && source[i] == '>') {
        advanceChar();

        push(TokenKind::Arrow, "->", tokenLine, tokenColumn);

        continue;
      }

      Error::meta(path.string(), tokenLine, tokenColumn,
                  "unexpected '-' in metadata");

    default:
      break;
    }

    Error::meta(path.string(), tokenLine, tokenColumn,
                std::string("unexpected character in metadata: '") + c + "'");
  }

  tokens.push_back({
      TokenKind::End,
      "",
      line,
      column,
  });
}

// ============================================================
// Parse root
// ============================================================

MetaReadResult MetaReader::parse() {
  MetaReadResult result;

  if (!matchIdentifier("module")) {
    fail("expected 'module' at beginning of metadata");
  }

  result.moduleName = expectIdentifier("expected module name").text;

  expect(TokenKind::Semicolon, "expected ';' after module declaration");

  while (!isAtEnd()) {
    const Token &kindToken = expectIdentifier("expected type declaration");

    if (kindToken.text == "trait") {
      result.meta.traits.push_back(readTrait());

      continue;
    }

    const auto typeKind = typeKindFromName(kindToken.text);

    if (!typeKind.has_value()) {
      fail(kindToken, "expected 'class', 'struct', "
                      "'enum' or 'trait'");
    }

    result.meta.types.push_back(readType(*typeKind));
  }

  return result;
}

// ============================================================
// Type
// ============================================================

TypeMeta MetaReader::readType(TypeKind kind) {
  TypeMeta result;

  result.kind = kind;

  QualifiedName name = readQualifiedName();

  result.name = std::move(name.name);
  result.path = std::move(name.path);

  if (match(TokenKind::Colon)) {
    if (kind != TypeKind::Class) {
      fail(previous(), "only class metadata may declare a parent");
    }

    result.parent = readTypeRef();
  }

  expect(TokenKind::LBrace, "expected '{' after type declaration");

  while (!check(TokenKind::RBrace)) {
    if (isAtEnd()) {
      fail("unexpected end of metadata inside type");
    }

    // enum variant has no access modifier.
    if (kind == TypeKind::Enum) {
      const bool looksLikeMember = checkIdentifier("public") ||
                                   checkIdentifier("protected") ||
                                   checkIdentifier("private");

      if (!looksLikeMember) {
        result.variants.push_back(readVariant());
        continue;
      }
    }

    const Token &modifierToken = expectIdentifier("expected access modifier");

    const auto modifier = modifierFromName(modifierToken.text);

    if (!modifier.has_value()) {
      fail(modifierToken, "expected access modifier");
    }

    const Token &member = expectIdentifier("expected field or method");

    if (member.text == "field") {
      if (kind == TypeKind::Enum) {
        fail(member, "enum metadata cannot contain fields");
      }

      FieldMeta field = readField();
      field.modifier = *modifier;

      result.fields.push_back(std::move(field));

      continue;
    }

    if (member.text == "method") {
      MethodMeta method = readMethod();
      method.modifier = *modifier;

      result.methods.push_back(std::move(method));

      continue;
    }

    fail(member, "expected 'field' or 'method'");
  }

  expect(TokenKind::RBrace, "expected '}' after type declaration");

  return result;
}

// ============================================================
// Trait
// ============================================================

TraitMeta MetaReader::readTrait() {
  TraitMeta result;

  QualifiedName name = readQualifiedName();

  result.name = std::move(name.name);
  result.path = std::move(name.path);

  expect(TokenKind::LBrace, "expected '{' after trait declaration");

  while (!check(TokenKind::RBrace)) {
    const Token &modifierToken = expectIdentifier("expected access modifier");

    const auto modifier = modifierFromName(modifierToken.text);

    if (!modifier.has_value()) {
      fail(modifierToken, "expected access modifier");
    }

    const Token &member = expectIdentifier("expected trait method");

    if (member.text != "method") {
      fail(member, "trait metadata may contain methods only");
    }

    MethodMeta method = readMethod();
    method.modifier = *modifier;

    result.methods.push_back(std::move(method));
  }

  expect(TokenKind::RBrace, "expected '}' after trait");

  return result;
}

// ============================================================
// Field
// ============================================================

FieldMeta MetaReader::readField() {
  FieldMeta result;

  result.name = expectIdentifier("expected field name").text;

  expect(TokenKind::Colon, "expected ':' after field name");

  result.type = readTypeRef();

  expect(TokenKind::Semicolon, "expected ';' after field");

  return result;
}

// ============================================================
// Method
// ============================================================

MethodMeta MetaReader::readMethod() {
  MethodMeta result;

  result.name = expectIdentifier("expected method name").text;

  expect(TokenKind::LParen, "expected '(' after method name");

  if (!check(TokenKind::RParen)) {
    do {
      result.params.push_back(readParam());
    } while (match(TokenKind::Comma));
  }

  expect(TokenKind::RParen, "expected ')' after method parameters");

  expect(TokenKind::Arrow, "expected '->' after method parameters");

  result.returnType = readTypeRef();

  if (matchIdentifier("init")) {
    expect(TokenKind::LBracket, "expected '[' after initializer metadata");

    if (!check(TokenKind::RBracket)) {
      do {
        result.initializedFields.push_back(
            expectIdentifier("expected initialized field name").text);
      } while (match(TokenKind::Comma));
    }

    expect(TokenKind::RBracket, "expected ']' after initialized field list");
  }

  expect(TokenKind::Semicolon, "expected ';' after method");

  return result;
}

// ============================================================
// Parameter
// ============================================================

ParamMeta MetaReader::readParam() {
  ParamMeta result;

  result.name = expectIdentifier("expected parameter name").text;

  expect(TokenKind::Colon, "expected ':' after parameter name");

  result.type = readTypeRef();

  if (match(TokenKind::Equal)) {
    result.defaultValue = readDefaultValue();
  }

  return result;
}

// ============================================================
// Enum variant
// ============================================================

EnumVariantMeta MetaReader::readVariant() {
  EnumVariantMeta result;

  result.name = expectIdentifier("expected enum variant name").text;

  if (match(TokenKind::LBracket)) {
    result.payload = readTypeRef();

    expect(TokenKind::RBracket, "expected ']' after variant payload");
  }

  expect(TokenKind::Semicolon, "expected ';' after enum variant");

  return result;
}

// ============================================================
// TypeRef
// ============================================================

TypeRef MetaReader::readTypeRef() {
  TypeRef result;

  QualifiedName name = readQualifiedName();

  // Builtin은 path를 가질 수 없음.
  if (name.path.segments.empty()) {
    const auto builtin = builtInFromName(name.name);

    if (builtin.has_value()) {
      result.kind = TypeRefKind::BuiltIn;
      result.builtIn = *builtin;
    } else {
      result.kind = TypeRefKind::Declared;
      result.name = std::move(name.name);
      result.path = std::move(name.path);
    }
  } else {
    result.kind = TypeRefKind::Declared;
    result.name = std::move(name.name);
    result.path = std::move(name.path);
  }

  // ------------------------------------------------------------
  // generic
  // ------------------------------------------------------------

  if (match(TokenKind::LAngle)) {
    if (result.kind == TypeRefKind::BuiltIn) {
      fail(previous(), "builtin type cannot have generic arguments");
    }

    result.kind = TypeRefKind::Generic;

    if (check(TokenKind::RAngle)) {
      fail(peek(), "generic type requires at least one argument");
    }

    do {
      result.args.push_back(readTypeRef());
    } while (match(TokenKind::Comma));

    expect(TokenKind::RAngle, "expected '>' after generic arguments");
  }

  // ------------------------------------------------------------
  // array
  // ------------------------------------------------------------

  if (match(TokenKind::LBracket)) {
    const Token &size = expect(TokenKind::Integer, "expected array size");

    if (!size.text.empty() && size.text.front() == '-') {
      fail(size, "array size cannot be negative");
    }

    llvm::APInt arraySize(64, size.text, 10);

    expect(TokenKind::RBracket, "expected ']' after array size");

    TypeRef element = std::move(result);

    result = TypeRef{};
    result.kind = TypeRefKind::Array;
    result.args.push_back(std::move(element));

    result.arraySize = std::move(arraySize);
  }

  return result;
}

// ============================================================
// Default value
// ============================================================

DefaultValueMeta MetaReader::readDefaultValue() {
  // Every default value stores its resolved type explicitly.
  //
  // Format:
  //   <resolved-type> : <value>
  //
  // Examples:
  //   i32: 10
  //   bool: true
  //   vec::vec: vec::vec(i32: 10, i32: 1)
  //   Vector<i32>: Vector<i32>(i32: 10)

  DefaultValueMeta result;

  result.resolvedType = readTypeRef();

  expect(TokenKind::Colon, "expected ':' after default value resolved type");

  if (check(TokenKind::Integer) || check(TokenKind::Float) ||
      check(TokenKind::String) || check(TokenKind::Char) ||
      checkIdentifier("true") || checkIdentifier("false")) {
    result.kind = DefaultValueKind::Literal;
    result.literal = readLiteral();

    return result;
  }

  result.kind = DefaultValueKind::StructInit;
  result.type = readTypeRef();

  expect(TokenKind::LParen, "expected '(' after default init type");

  if (!check(TokenKind::RParen)) {
    do {
      result.args.push_back(readDefaultValue());
    } while (match(TokenKind::Comma));
  }

  expect(TokenKind::RParen, "expected ')' after default init");

  return result;
}

// ============================================================
// Literal
// ============================================================

ResolvedLit MetaReader::readLiteral() {
  ResolvedLit result;
  result.type = nullptr;

  if (matchIdentifier("true")) {
    result.value = true;
    return result;
  }

  if (matchIdentifier("false")) {
    result.value = false;
    return result;
  }

  if (match(TokenKind::Integer)) {
    const std::string &text = previous().text;

    const bool negative = !text.empty() && text.front() == '-';

    std::string_view magnitude = text;

    if (negative) {
      magnitude.remove_prefix(1);
    }

    // Metadata reader 단계에서는 실제 TypeSymbol이 아직 없으므로
    // 충분히 넓게 저장한다.
    //
    // 후속 symbol 복원 단계에서 expected TypeRef에 맞춰
    // 실제 builtin 타입으로 확정하면 된다.
    llvm::APInt value(128, magnitude, 10);

    if (negative) {
      value = -value;
    }

    result.value = IntPayload(std::move(value));

    return result;
  }

  if (match(TokenKind::Float)) {
    llvm::APFloat value(llvm::APFloat::IEEEquad());

    auto status = value.convertFromString(previous().text,
                                          llvm::APFloat::rmNearestTiesToEven);

    if (!status) {
      Error::meta(path.string(), previous().line, previous().column,
                  "failed to parse floating-point literal");
    }
    result.value = FloatPayload(std::move(value));

    return result;
  }

  if (match(TokenKind::Char)) {
    result.value = CharPayload(decodeChar(previous().text));

    return result;
  }

  if (match(TokenKind::String)) {
    result.value = StringPayload(decodeString(previous().text));

    return result;
  }

  fail(peek(), "expected metadata literal");
}

// ============================================================
// Qualified name
// ============================================================

MetaReader::QualifiedName MetaReader::readQualifiedName() {
  QualifiedName result;

  std::vector<std::string> segments;

  segments.push_back(expectIdentifier("expected type name").text);

  while (match(TokenKind::DoubleColon)) {
    segments.push_back(expectIdentifier("expected name after '::'").text);
  }

  if (segments.size() == 1) {
    result.name = std::move(segments.front());

    return result;
  }

  result.name = std::move(segments.back());

  segments.pop_back();

  result.path.segments = std::move(segments);

  return result;
}

// ============================================================
// Builtin
// ============================================================

std::optional<BuiltInType> MetaReader::builtInFromName(std::string_view name) {
  for (const BuiltinEntry &entry : builtinEntries) {
    if (name == entry.name) {
      return entry.type;
    }
  }

  if (name == "void") {
    return BuiltInType::VOID;
  }

  if (name == "fi") {
    return BuiltInType::FI;
  }

  return std::nullopt;
}

// ============================================================
// Type kind
// ============================================================

std::optional<TypeKind> MetaReader::typeKindFromName(std::string_view name) {
  if (name == "class") {
    return TypeKind::Class;
  }

  if (name == "struct") {
    return TypeKind::Struct;
  }

  if (name == "enum") {
    return TypeKind::Enum;
  }

  return std::nullopt;
}

// ============================================================
// Access modifier
// ============================================================

std::optional<AModifier> MetaReader::modifierFromName(std::string_view name) {
  if (name == "public") {
    return AModifier::PUBLIC;
  }

  if (name == "protected") {
    return AModifier::PROTECTED;
  }

  if (name == "private") {
    return AModifier::PRIVATE;
  }

  return std::nullopt;
}

// ============================================================
// Tokens
// ============================================================

const MetaReader::Token &MetaReader::peek(std::size_t offset) const {
  const std::size_t index = std::min(current + offset, tokens.size() - 1);

  return tokens[index];
}

const MetaReader::Token &MetaReader::previous() const {
  return tokens[current - 1];
}

bool MetaReader::isAtEnd() const { return peek().kind == TokenKind::End; }

bool MetaReader::check(TokenKind kind) const { return peek().kind == kind; }

bool MetaReader::checkIdentifier(std::string_view text) const {
  return peek().kind == TokenKind::Identifier && peek().text == text;
}

bool MetaReader::match(TokenKind kind) {
  if (!check(kind)) {
    return false;
  }

  advance();
  return true;
}

bool MetaReader::matchIdentifier(std::string_view text) {
  if (!checkIdentifier(text)) {
    return false;
  }

  advance();
  return true;
}

const MetaReader::Token &MetaReader::advance() {
  if (!isAtEnd()) {
    ++current;
  }

  return previous();
}

const MetaReader::Token &MetaReader::expect(TokenKind kind,
                                            std::string_view message) {
  if (!check(kind)) {
    fail(peek(), message);
  }

  return advance();
}

const MetaReader::Token &
MetaReader::expectIdentifier(std::string_view message) {
  return expect(TokenKind::Identifier, message);
}

// ============================================================
// Error
// ============================================================

[[noreturn]]
void MetaReader::fail(const Token &token, std::string_view message) const {
  Error::meta(path.string(), token.line, token.column, std::string(message));
}

[[noreturn]]
void MetaReader::fail(std::string_view message) const {
  fail(peek(), message);
}

// ============================================================
// Literal decoding
// ============================================================

uint32_t MetaReader::decodeChar(std::string_view text) {
  if (text.empty()) {
    throw std::runtime_error("empty metadata char literal");
  }

  std::size_t index = 0;

  if (text[index] == '\\') {
    return decodeEscape(text, index);
  }

  const unsigned char first = static_cast<unsigned char>(text[index]);

  // ASCII
  if (first < 0x80) {
    if (text.size() != 1) {
      throw std::runtime_error("metadata char literal contains "
                               "multiple characters");
    }

    return first;
  }

  // UTF-8
  uint32_t codePoint = 0;
  std::size_t count = 0;

  if ((first & 0xE0) == 0xC0) {
    codePoint = first & 0x1F;
    count = 2;
  } else if ((first & 0xF0) == 0xE0) {
    codePoint = first & 0x0F;
    count = 3;
  } else if ((first & 0xF8) == 0xF0) {
    codePoint = first & 0x07;
    count = 4;
  } else {
    throw std::runtime_error("invalid UTF-8 in metadata char");
  }

  if (text.size() != count) {
    throw std::runtime_error("metadata char literal contains "
                             "multiple code points");
  }

  for (std::size_t i = 1; i < count; ++i) {
    const unsigned char byte = static_cast<unsigned char>(text[i]);

    if ((byte & 0xC0) != 0x80) {
      throw std::runtime_error("invalid UTF-8 continuation byte");
    }

    codePoint = (codePoint << 6) | (byte & 0x3F);
  }

  return codePoint;
}

std::vector<uint32_t> MetaReader::decodeString(std::string_view text) {
  std::vector<uint32_t> result;

  std::size_t i = 0;

  while (i < text.size()) {
    if (text[i] == '\\') {
      result.push_back(decodeEscape(text, i));

      continue;
    }

    const unsigned char first = static_cast<unsigned char>(text[i]);

    if (first < 0x80) {
      result.push_back(first);
      ++i;
      continue;
    }

    uint32_t codePoint = 0;
    std::size_t count = 0;

    if ((first & 0xE0) == 0xC0) {
      codePoint = first & 0x1F;
      count = 2;
    } else if ((first & 0xF0) == 0xE0) {
      codePoint = first & 0x0F;
      count = 3;
    } else if ((first & 0xF8) == 0xF0) {
      codePoint = first & 0x07;
      count = 4;
    } else {
      throw std::runtime_error("invalid UTF-8 in metadata string");
    }

    if (i + count > text.size()) {
      throw std::runtime_error("truncated UTF-8 metadata string");
    }

    for (std::size_t j = 1; j < count; ++j) {
      const unsigned char byte = static_cast<unsigned char>(text[i + j]);

      if ((byte & 0xC0) != 0x80) {
        throw std::runtime_error("invalid UTF-8 continuation byte");
      }

      codePoint = (codePoint << 6) | (byte & 0x3F);
    }

    result.push_back(codePoint);
    i += count;
  }

  return result;
}

uint32_t MetaReader::decodeEscape(std::string_view text, std::size_t &index) {
  if (index >= text.size() || text[index] != '\\') {
    throw std::runtime_error("invalid metadata escape");
  }

  ++index;

  if (index >= text.size()) {
    throw std::runtime_error("incomplete metadata escape");
  }

  const char c = text[index++];

  switch (c) {
  case 'n':
    return '\n';

  case 'r':
    return '\r';

  case 't':
    return '\t';

  case '\\':
    return '\\';

  case '\'':
    return '\'';

  case '"':
    return '"';

  case 'u': {
    if (index >= text.size() || text[index] != '{') {
      throw std::runtime_error("expected '{' after \\u");
    }

    ++index;

    uint32_t value = 0;
    bool hasDigit = false;

    while (index < text.size() && text[index] != '}') {
      const int digit = hexValue(text[index]);

      if (digit < 0) {
        throw std::runtime_error("invalid unicode escape");
      }

      hasDigit = true;

      if (value > (0x10FFFFu - static_cast<uint32_t>(digit)) / 16u) {
        throw std::runtime_error("unicode escape out of range");
      }

      value = value * 16u + static_cast<uint32_t>(digit);

      ++index;
    }

    if (!hasDigit || index >= text.size() || text[index] != '}') {
      throw std::runtime_error("unterminated unicode escape");
    }

    ++index;

    if (value > 0x10FFFFu || (value >= 0xD800u && value <= 0xDFFFu)) {
      throw std::runtime_error("invalid unicode code point");
    }

    return value;
  }

  default:
    throw std::runtime_error("unknown metadata escape");
  }
}

int MetaReader::hexValue(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }

  if (c >= 'a' && c <= 'f') {
    return 10 + c - 'a';
  }

  if (c >= 'A' && c <= 'F') {
    return 10 + c - 'A';
  }

  return -1;
}