#include "util/Error.h"
#include "util/Printor.h"
#include <iostream>
#include <sstream>
#include <stdexcept>

[[noreturn]]
void Error::diagnostic(const Token &token, const std::string &message) {
  std::ostringstream oss;
  oss << "[error] " << token.line << ":" << token.col << ": " << message;

  throw std::runtime_error(oss.str());
}

[[noreturn]]
void Error::diagnostic(const Token &primary, const std::string &message,
                       const Token &secondary, const std::string &note) {
  std::ostringstream oss;
  oss << "[error] " << primary.line << ":" << primary.col << ": " << message
      << "\n"
      << "  note: " << secondary.line << ":" << secondary.col << ": " << note;

  throw std::runtime_error(oss.str());
}

[[noreturn]]
void Error::symbol(const Symbol &symbol, const std::string &message) {
  std::ostringstream oss;
  oss << "[error] <symbol '" << symbol.name << "'>: " << message;

  throw std::runtime_error(oss.str());
}

[[noreturn]]
void Error::internal(const std::string &message) {
  std::cerr << "[internal compiler error]\n";
  std::cerr << message << "\n";

  Printor::printStackTrace();

  throw std::runtime_error(message);
}

[[noreturn]]
void Error::fatal(ErrorCategory category, const std::string &message) {
  std::cerr << "[fatal";

  if (category == ErrorCategory::Sanitizer)
    std::cerr << ":asan";
  else if (category == ErrorCategory::Internal)
    std::cerr << ":internal";

  std::cerr << "] " << message << "\n";

  Printor::printStackTrace();
  throw std::runtime_error(message);
}
