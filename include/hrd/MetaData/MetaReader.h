#pragma once

#include "hrd/MetaData/MetaData.h"

#include <filesystem>
#include <istream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct MetaReadResult {
  std::string moduleName;
  ModuleMeta meta;
};

class MetaReader {
public:
  MetaReadResult read(std::istream &in, const std::filesystem::path &path);

private:
  enum class TokenKind {
    Identifier,
    Integer,
    Float,
    String,
    Char,

    LBrace,
    RBrace,
    LParen,
    RParen,
    LBracket,
    RBracket,
    LAngle,
    RAngle,

    Colon,
    DoubleColon,
    Comma,
    Semicolon,
    Equal,
    Arrow,

    End,
  };

  struct Token {
    TokenKind kind;
    std::string text;

    std::size_t line = 1;
    std::size_t column = 1;
  };

private:
  std::filesystem::path path;

  std::vector<Token> tokens;
  std::size_t current = 0;

  // ============================================================
  // Lexer
  // ============================================================

  void tokenize(std::string_view source);

  // ============================================================
  // Parser
  // ============================================================

  MetaReadResult parse();

  TypeMeta readType(TypeKind kind);
  TraitMeta readTrait();

  FieldMeta readField();
  MethodMeta readMethod();
  ParamMeta readParam();
  EnumVariantMeta readVariant();

  TypeRef readTypeRef();

  DefaultValueMeta readDefaultValue();
  ResolvedLit readLiteral();

  // ============================================================
  // Names / path
  // ============================================================

  struct QualifiedName {
    SourcePath path;
    std::string name;
  };

  QualifiedName readQualifiedName();

  // ============================================================
  // Conversion
  // ============================================================

  static std::optional<BuiltInType> builtInFromName(std::string_view name);

  static std::optional<TypeKind> typeKindFromName(std::string_view name);

  static std::optional<AModifier> modifierFromName(std::string_view name);

  // ============================================================
  // Token helpers
  // ============================================================

  const Token &peek(std::size_t offset = 0) const;
  const Token &previous() const;

  bool isAtEnd() const;

  bool check(TokenKind kind) const;
  bool checkIdentifier(std::string_view text) const;

  bool match(TokenKind kind);
  bool matchIdentifier(std::string_view text);

  const Token &advance();

  const Token &expect(TokenKind kind, std::string_view message);

  const Token &expectIdentifier(std::string_view message);

  [[noreturn]]
  void fail(const Token &token, std::string_view message) const;

  [[noreturn]]
  void fail(std::string_view message) const;

  // ============================================================
  // Literal helpers
  // ============================================================

  static uint32_t decodeChar(std::string_view text);
  static std::vector<uint32_t> decodeString(std::string_view text);

  static uint32_t decodeEscape(std::string_view text, std::size_t &index);

  static int hexValue(char c);
};