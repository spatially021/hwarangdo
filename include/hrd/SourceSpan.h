#pragma once

#include <string>

struct SourceSpan {
  std::string path = "[MAIN]";
  int lineStart = -1;
  int colStart = 0;
  int lineEnd = 0;
  int colEnd = 0;
};

inline SourceSpan makeSpan(const SourceSpan &a, const SourceSpan &b) {
  SourceSpan s;
  s.path = a.path;

  s.lineStart = a.lineStart;
  s.colStart = a.colStart;

  s.lineEnd = b.lineEnd;
  s.colEnd = b.colEnd;

  return s;
}
