#include "util/Error.h"
#include "SemanticAnalyzer/symbol/Symbol.h"
#include "Token.h"
#include "util/Printor.h"
#include <iostream>
#include <sstream>
#include <stdexcept>

#ifndef NDEBUG
#define HGM_DEBUG 1
#else
#define HGM_DEBUG 0
#endif

[[noreturn]]
inline void throwError(const std::string &msg) {
#if HGM_DEBUG
  Printor::printStackTrace();
#endif
  throw std::runtime_error(msg);
}

[[noreturn]]
void Error::diagnostic(const Token &token, const std::string &message) {
  diagnostic(token.span, message);
}

[[noreturn]]
void Error::diagnostic(const SourceSpan &span, const std::string &message) {
  std::ostringstream oss;

  oss << span.path << ":" << span.lineStart << ":" << span.colStart
      << ": error: " << message;

  throwError(oss.str());
}

[[noreturn]]
void Error::symbol(const Symbol &symbol, const std::string &message) {
  std::ostringstream oss;
  oss << "[error] <symbol '" << symbol.name << "'>: " << message;

  throwError(oss.str());
}
[[noreturn]]
void Error::internal(const std::string &message) {
  std::cerr << "[internal compiler error]\n";
  std::cerr << message << "\n";

  throwError(message);
}
[[noreturn]]
void Error::internal(const Token &token, const std::string &message) {
  internal(token.span, message);
}

[[noreturn]] void Error::internal(const SourceSpan &span, str message) {
  std::ostringstream oss;

  oss << "[internal compiler error]\n" << span.path << " ";

  if (span.lineStart == span.lineEnd) {
    if (span.colStart == span.colEnd) {
      // point
      oss << span.lineStart << ":" << span.colStart;
    } else {
      // same line range
      oss << span.lineStart << ":" << span.colStart << "-" << span.colEnd;
    }
  } else {
    // multi-line range
    oss << span.lineStart << ":" << span.colStart << " - " << span.lineEnd
        << ":" << span.colEnd;
  }

  oss << ": " << message;

  throwError(oss.str());
}

[[noreturn]]
void Error::fatal(ErrorCategory category, const std::string &message) {
  std::cerr << "[fatal";

  if (category == ErrorCategory::Sanitizer)
    std::cerr << ":asan";
  else if (category == ErrorCategory::Internal)
    std::cerr << ":internal";

  std::cerr << "] " << message << "\n";

  throwError(message);
}