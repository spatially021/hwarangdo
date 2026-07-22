#include "hrd/util/diagnostic/DiagnosticEngine.h"
#include "hrd/util/diagnostic/Diagnostic.h"

#include <utility>

DiagnosticEngine::DiagnosticEngine(std::unique_ptr<DiagnosticRenderer> r,
                                   std::size_t m)
    : renderer(std::move(r)), maxErrors(m) {}

void DiagnosticEngine::emit(Diagnostic diagnostic) {
  switch (diagnostic.level) {
  case DiagnosticLevel::Error:
    ++errors;
    break;

  case DiagnosticLevel::Warning:
    ++warnings;
    break;
  }

  renderer->render(diagnostic);
}

bool DiagnosticEngine::hasErrors() const { return errors > 0; }

bool DiagnosticEngine::reachedErrorLimit() const { return errors >= maxErrors; }

std::size_t DiagnosticEngine::errorCount() const { return errors; }

std::size_t DiagnosticEngine::warningCount() const { return warnings; }