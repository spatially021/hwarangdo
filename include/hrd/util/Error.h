#pragma once

#include "hrd/SourceSpan.h"
#include <string>

struct Token;

class Error {
  using str = const std::string &;

public:
  enum class ErrorCategory { User, Semantic, Internal, Sanitizer };

  [[noreturn]] static void diagnostic(const Token &token, str message);
  [[noreturn]] static void diagnostic(const SourceSpan &span, str message);
  [[noreturn]] static void internal(str message);
  [[noreturn]] static void internal(const Token &token, str meessage);
  [[noreturn]] static void internal(const SourceSpan &span, str message);
  [[noreturn]] static void fatal(ErrorCategory category,
                                 const std::string &message);
  static void warn(SourceSpan span, str message);
  static void warn(Token &token, str message);
};