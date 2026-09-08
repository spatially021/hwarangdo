
#include "hrd/diagnostic/DiagnosticCatalog.h"
#include "hrd/diagnostic/Diagnostic.h"
#include "hrd/util/Error.h"

#define DIAG(code, level, id, msg)                                             \
  {DiagnosticCode::code, {DiagnosticLevel::level, msg, id}},

const std::unordered_map<DiagnosticCode, Diagnostic>
    DiagnosticCatalog::definitions = {

#include "hrd/diagnostic/def/BuilderDef.def"
#include "hrd/diagnostic/def/DriverDef.def"
#include "hrd/diagnostic/def/HIRDef.def"
#include "hrd/diagnostic/def/ImportDef.def"
#include "hrd/diagnostic/def/LexerDef.def"
#include "hrd/diagnostic/def/LinkerDef.def"
#include "hrd/diagnostic/def/ParserDef.def"
#include "hrd/diagnostic/def/ResolverDef.def"

};

#undef DIAG

const Diagnostic &DiagnosticCatalog::get(DiagnosticCode code) {
  auto it = definitions.find(code);
  if (it == definitions.end()) {
    Error::internal("unknown diagnostic code");
  }
  return it->second;
}