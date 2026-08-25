#include "hrd/AST/Expr.h"
#include "hrd/SemanticAnalyzer/ResolvedLit.h"
#include "hrd/SemanticAnalyzer/Resolver.h"
#include "hrd/diagnostic/Diagnostic.h"
#include "hrd/util/Error.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Error.h"
#include <llvm/ADT/APFloat.h>

const string max32 = "2147483647";
const string max64 = "9223372036854775807";
const string max128 = "170141183460469231731687303715884105727";

bool Resolver::fitsFloatRange(const llvm::APFloat &base,
                              const llvm::fltSemantics &sem) {
  llvm::APFloat temp = base;
  bool losesInfo = false;
  auto status =
      temp.convert(sem, llvm::APFloat::rmNearestTiesToEven, &losesInfo);

  return (status & llvm::APFloat::opOverflow) == 0;
}

llvm::APFloat Resolver::convertFloatTo(const llvm::APFloat &base,
                                       const llvm::fltSemantics &sem) {
  llvm::APFloat temp = base;
  bool losesInfo = false;
  auto status =
      temp.convert(sem, llvm::APFloat::rmNearestTiesToEven, &losesInfo);

  if (status & llvm::APFloat::opOverflow) {
    llvm_unreachable("convertFloatTo called with overflowing target semantics");
  }

  return temp;
}

ResolvedLit Resolver::resolveLitFloat(LiteralExpr *expr) {
  std::string s = expr->value;

  llvm::APFloat base(llvm::APFloat::IEEEquad());
  auto parseResult = base.convertFromString(llvm::StringRef(s),
                                            llvm::APFloat::rmNearestTiesToEven);

  if (!parseResult) {
    Error::internal(expr->span, "invalid floating-point literal");
  }

  auto parseStatus = *parseResult;

  if (parseStatus & llvm::APFloat::opInvalidOp) {
    Error::internal(expr->span, "invalid floating-point literal");
  }
  if (parseStatus & llvm::APFloat::opOverflow) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S033);
    dia.labels = {
        {expr->span, "value exceeds the supported floating-point range", true}};
    dia.notes = {{"the largest supported floating-point type is float:f128"}};
    dia.helps = {{"use a smaller floating-point value"}};
    engine.emit(dia);
    recover.recover();
  }

  struct Candidate {
    unsigned bits;
    const char *name;
    const llvm::fltSemantics &sem;
  };

  static const Candidate candidates[] = {
      {32, "f32", llvm::APFloat::IEEEsingle()},
      {64, "f64", llvm::APFloat::IEEEdouble()},
      {128, "f128", llvm::APFloat::IEEEquad()},
  };

  for (const auto &cand : candidates) {
    if (!fitsFloatRange(base, cand.sem)) {
      continue;
    }

    ResolvedLit resolvedLit;
    resolvedLit.type = table.getType(cand.name);
    resolvedLit.value = FloatPayload(convertFloatTo(base, cand.sem));
    return resolvedLit;
  }

  auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S033);
  dia.labels = {
      {expr->span, "value exceeds the supported floating-point range", true}};
  dia.notes = {{"the largest supported floating-point type is float:f128"}};
  dia.helps = {{"use a smaller floating-point value"}};
  engine.emit(dia);
  recover.recover();
  return {};
}
