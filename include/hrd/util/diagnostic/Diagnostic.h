#pragma once

#include "hrd/SourceSpan.h"
#include <optional>
#include <vector>
enum class DiagnosticLevel {
  Error,
  Warning,
};

enum class DiagnosticCode {
  HRD_L001,
  HRD_L002,
  HRD_L003,
  HRD_L004,

  HRD_P001,
  HRD_P002,
  HRD_P003,
  HRD_P004,
  HRD_P005,
  HRD_P006,
  HRD_P007,
  HRD_P008,
  HRD_P009,
  HRD_P010,
  HRD_P011,
  HRD_P012,
  HRD_P013,
  HRD_P014,
  HRD_P015,
  HRD_P016,
  HRD_P017,
  HRD_P018,
  HRD_P019,
  HRD_P020,
  HRD_P021,
  HRD_P022,
  HRD_P023,
  HRD_P024,
  HRD_P025,

  HRD_P026,
  HRD_P027,
  HRD_P028,
  HRD_P029,
  HRD_P030,
  HRD_P031,
  HRD_P032,
  HRD_P033,
  HRD_P034,
  HRD_P035,

  HRD_P036,
  HRD_P037,
  HRD_P038,
  HRD_P039,

  HRD_P040,
  HRD_P041,
  HRD_P042,
  HRD_P043,
  HRD_P044,
  HRD_P045,
  HRD_P046,
  HRD_P047,
  HRD_P048,
  HRD_P049,
  HRD_P050,
  HRD_P051,
  HRD_P052,
  HRD_P053,
  HRD_P054,
  HRD_P055,
  HRD_P056,
  HRD_P057,

  HRD_S001,

};

struct DiagnosticLabel {
  SourceSpan span;
  std::string message;
  bool primary = false;
};

struct Diagnostic {
  DiagnosticLevel level;
  std::string message;
  std::optional<std::string> code;

  std::vector<DiagnosticLabel> labels;
  std::vector<std::string> notes;
  std::vector<std::string> helps;

  Diagnostic(DiagnosticLevel l, std::string m, std::string c)
      : level(l), message(std::move(m)), code(std::move(c)) {}
};