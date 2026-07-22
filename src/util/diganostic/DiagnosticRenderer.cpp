#include "hrd/util/diagnostic/DiagnosticRenderer.h"

#include <string_view>

namespace {

std::string_view levelName(DiagnosticLevel level) {
  switch (level) {
  case DiagnosticLevel::Error:
    return "error";

  case DiagnosticLevel::Warning:
    return "warning";
  }

  return "unknown";
}

} // namespace

TerminalDiagnosticRenderer::TerminalDiagnosticRenderer(std::ostream &o)
    : out(o) {}

void TerminalDiagnosticRenderer::render(const Diagnostic &diagnostic) {
  out << levelName(diagnostic.level);

  if (diagnostic.code.has_value()) {
    out << '[' << *diagnostic.code << ']';
  }

  out << ": " << diagnostic.message << '\n';

  for (const auto &label : diagnostic.labels) {
    out << "  --> " << label.span.path << ':' << label.span.lineStart << ':'
        << label.span.colStart << '\n';

    if (!label.message.empty()) {
      out << "      ";

      if (!label.primary) {
        out << "note: ";
      }

      out << label.message << '\n';
    }
  }

  for (const auto &note : diagnostic.notes) {
    out << "  note: " << note << '\n';
  }

  for (const auto &help : diagnostic.helps) {
    out << "  help: " << help << '\n';
  }
}