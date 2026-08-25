#pragma once

#include "hrd/SourceSpan.h"
#include <string>

struct Token;

class Error {
  using str = const std::string &;

public:
  [[noreturn]] static void internal(str message);
  [[noreturn]] static void internal(const Token &token, str message);
  [[noreturn]] static void internal(const SourceSpan &span, str message);

  static void warn(SourceSpan span, str message);
  static void warn(Token &token, str message);

  [[noreturn]] static void diagnostic(const SourceSpan &span,
                                      const std::string &message);
  [[noreturn]] static void diagnostic(const Token &token,
                                      const std::string &message);

  [[noreturn]] static void meta(str message);
  [[noreturn]] static void meta(const std::string &path, str message);
  [[noreturn]] static void meta(const std::string &path, std::size_t line,
                                std::size_t column, str message);
};