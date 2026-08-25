#include "hrd/diagnostic/DiagnosticRenderer.h"
#include "hrd/Color.h"

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
  out << '\r' << "\033[2K";
  out.flush();
  switch (diagnostic.level) {

  case DiagnosticLevel::Error: {
    out << Color::RED;
    break;
  }
  case DiagnosticLevel::Warning: {
    out << Color::YELLOW;
    break;
  }
  }
  out << levelName(diagnostic.level);

  if (diagnostic.code.has_value()) {
    out << '[' << *diagnostic.code << ']';
  }

  out << ": " << diagnostic.message << '\n';

  for (const auto &label : diagnostic.labels) {
    if (label.span.has_value()) {
      auto span = &label.span.value();
      out << "  --> " << span->path << ':' << span->lineStart << ':'
          << span->colStart << '\n';

      if (!label.message.empty()) {
        out << "      ";

        if (!label.primary) {
          out << "note: ";
        }

        out << label.message << '\n';
      }
    }
  }

  for (const auto &note : diagnostic.notes) {
    out << "  note: " << note << '\n';
  }

  for (const auto &help : diagnostic.helps) {
    out << "  help: " << help << '\n';
  }

  out << Color::RESET;
}