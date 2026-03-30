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
  std::ostringstream oss;
  oss << "[error] " << token.path << " " << token.line << ":" << token.col
      << ": " << message;

  throwError(oss.str());
}

[[noreturn]]
void Error::diagnostic(const Token &primary, const std::string &message,
                       const Token &secondary, const std::string &note) {
  std::ostringstream oss;
  oss << "[error] " << primary.path << " " << primary.line << ":" << primary.col
      << ": " << message << "\n"
      << "  note: " << primary.path << " " << secondary.line << ":"
      << secondary.col << ": " << note;

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
  std::ostringstream oss;
  oss << "[internal compiler error]\n"
      << token.path << " " << token.line << ":" << token.col << ": " << message;
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