#pragma once

#include "Diagnostic.h"
#include <ostream>
class DiagnosticRenderer {
public:
  virtual ~DiagnosticRenderer() = default;

  virtual void render(const Diagnostic &diagnostic) = 0;
};

class TerminalDiagnosticRenderer final : public DiagnosticRenderer {
public:
  explicit TerminalDiagnosticRenderer(std::ostream &out);

  void render(const Diagnostic &diagnostic) override;

private:
  std::ostream &out;
};