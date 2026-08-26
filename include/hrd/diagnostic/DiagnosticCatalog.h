#pragma once

#include "Diagnostic.h"
#include <unordered_map>

class DiagnosticCatalog {
public:
  static const Diagnostic &get(DiagnosticCode code);

private:
  static const std::unordered_map<DiagnosticCode, Diagnostic> definitions;
};