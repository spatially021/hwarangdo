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
  HRD_L005,
  HRD_L006,

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

  HRD_P058,
  HRD_P059,

  HRD_S001,
  HRD_S002,
  HRD_S003,
  HRD_S004,
  HRD_S005,
  HRD_S006,
  HRD_S007,

  HRD_S008,
  HRD_S009,
  HRD_S010,
  HRD_S011,
  HRD_S012,
  HRD_S013,
  HRD_S014,
  HRD_S015,
  HRD_S016,
  HRD_S017,
  HRD_S018,

  HRD_S019,
  HRD_S020,
  HRD_S021,
  HRD_S022,
  HRD_S023,
  HRD_S024,
  HRD_S025,
  HRD_S026,
  HRD_S027,
  HRD_S028,
  HRD_S029,
  HRD_S030,
  HRD_S031,
  HRD_S032,

  HRD_S033,

  HRD_S034,
  HRD_S035,
  HRD_S036,
  HRD_S037,
  HRD_S038,
  HRD_S039,
  HRD_S040,
  HRD_S041,
  HRD_S042,
  HRD_S043,
  HRD_S044,
  HRD_S045,
  HRD_S046,
  HRD_S047,

  HRD_S048,
  HRD_S049,
  HRD_S050,
  HRD_S051,
  HRD_S052,
  HRD_S053,

  HRD_S054,
  HRD_S055,
  HRD_S056,
  HRD_S057,
  HRD_S058,
  HRD_S059,
  HRD_S060,
  HRD_S061,
  HRD_S062,
  HRD_S063,
  HRD_S064,
  HRD_S065,

  HRD_S066,
  HRD_S067,
  HRD_S068,
  HRD_S069,
  HRD_S070,
  HRD_S071,

  HRD_S072,
  HRD_S073,
  HRD_S074,
  HRD_S075,
  HRD_S076,

  HRD_S077,
  HRD_S078,
  HRD_S079,
  HRD_S080,
  HRD_S081,
  HRD_S082,
  HRD_S083,
  HRD_S084,
  HRD_S085,
  HRD_S086,
  HRD_S087,
  HRD_S088,

  HRD_S089,
  HRD_S090,
  HRD_S091,
  HRD_S092,
  HRD_S093,
  HRD_S094,
  HRD_S095,
  HRD_S096,
  HRD_S097,
  HRD_S098,
  HRD_S099,
  HRD_S100,
  HRD_S101,
  HRD_S102,
  HRD_S103,

  HRD_S104,
  HRD_S105,
  HRD_S106,
  HRD_S107,
  HRD_S108,

  HRD_H001,
  HRD_H002,
  HRD_H003,
  HRD_H004,
  HRD_H005,
  HRD_H006,
  HRD_H007,

  HRD_H008,
  HRD_H009,
  HRD_H010,

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