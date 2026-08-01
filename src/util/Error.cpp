#include "hrd/util/Error.h"
#include "hrd/SourceSpan.h"
#include "hrd/Token.h"
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
  throw std::runtime_error(msg);
}

// [[noreturn]] void Error::diagnostic(const Token &token,
//                                     const std::string &message) {
//   diagnostic(token.span, message);
// }
// [[noreturn]] void Error::diagnostic(const SourceSpan &span,
//                                     const std::string &message) {
//   std::ostringstream oss;
//   oss << span.path << ":" << span.lineStart << ":" << span.colStart
//       << ": error: " << message;
//   throwError(oss.str());
// }

[[noreturn]] void Error::internal(const std::string &message) {
  std::cerr << "[internal compiler error]\n";
  std::cerr << message << "\n";
  throwError(message);
}
[[noreturn]] void Error::internal(const Token &token,
                                  const std::string &message) {
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
    // multi - line range
    oss << span.lineStart << ":" << span.colStart << " - " << span.lineEnd
        << ":" << span.colEnd;
  }
  oss << ": " << message;
  throwError(oss.str());
}

// void Error::warn(Token &token, str message) { warn(token.span, message); }
// void Error::warn(SourceSpan span, str message) {
//   std::cerr << "warning: " << message << "\n";
//   std::cerr << " -->               " << span.path << " : " << span.lineStart
//             << " : " << span.colStart << "\n";
// }