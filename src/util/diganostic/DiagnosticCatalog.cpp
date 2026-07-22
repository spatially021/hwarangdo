
#include "./hrd/./util/./diagnostic/./DiagnosticCatalog.h"
#include "hrd/util/Error.h"
#include "hrd/util/diagnostic/Diagnostic.h"

#define DIAG(code, level, id, msg)                                             \
  {DiagnosticCode::code, {DiagnosticLevel::level, msg, id}},

const std::unordered_map<DiagnosticCode, Diagnostic>
    DiagnosticCatalog::definitions = {

#include "hrd/util/diagnostic/def/BuilderDef.def"
#include "hrd/util/diagnostic/def/LexerDef.def"
#include "hrd/util/diagnostic/def/ParserDef.def"

};

#undef DIAG

const Diagnostic &DiagnosticCatalog::get(DiagnosticCode code) {
  auto it = definitions.find(code);
  if (it == definitions.end()) {
    Error::internal("unknown diagnostic code");
  }
  return it->second;
}