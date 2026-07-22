#pragma once

#include "hrd/util/diagnostic/Diagnostic.h"
#include "hrd/util/diagnostic/DiagnosticCatalog.h"
#include "hrd/util/diagnostic/DiagnosticRenderer.h"
#include <cstddef>
#include <memory>

class DiagnosticEngine {
public:
  explicit DiagnosticEngine(std::unique_ptr<DiagnosticRenderer> renderer,
                            std::size_t maxErrors = 20);

  void emit(Diagnostic diagnostic);

  [[nodiscard]]
  bool hasErrors() const;

  [[nodiscard]]
  bool reachedErrorLimit() const;

  [[nodiscard]]
  std::size_t errorCount() const;

  [[nodiscard]]
  std::size_t warningCount() const;

  Diagnostic makeDiagnostic(DiagnosticCode code) { return catalog->get(code); }

private:
  std::unique_ptr<DiagnosticRenderer> renderer;
  std::unique_ptr<DiagnosticCatalog> catalog =
      std::make_unique<DiagnosticCatalog>();
  std::size_t errors = 0;
  std::size_t warnings = 0;
  std::size_t maxErrors = 20;
};