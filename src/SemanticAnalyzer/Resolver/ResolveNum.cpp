#include "AST/Expr.h"
#include "SemanticAnalyzer/ResolvedLit.h"
#include "SemanticAnalyzer/Resolver.h"
#include "util/Error.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Error.h"
#include <cstddef>
#include <llvm/ADT/APFloat.h>

const string max32 = "2147483647";
const string max64 = "9223372036854775807";
const string max128 = "170141183460469231731687303715884105727";

static int compareUnsignedDecimal(const std::string &a, const std::string &b) {
  if (a.size() < b.size())
    return -1;
  if (a.size() > b.size())
    return 1;
  if (a < b)
    return -1;
  if (a > b)
    return 1;
  return 0;
}

ResolvedLit Resolver::resolveLitInt(LiteralExpr *expr) {
  std::string s = expr->value;

  size_t pos = s.find_first_not_of('0');
  if (pos == std::string::npos) {
    s = "0";
  } else {
    s = s.substr(pos);
  }

  unsigned bits = 0;

  if (compareUnsignedDecimal(s, max32) <= 0) {
    bits = 32;
  } else if (compareUnsignedDecimal(s, max64) <= 0) {
    bits = 64;
  } else if (compareUnsignedDecimal(s, max128) <= 0) {
    bits = 128;
  } else {
    Error::diagnostic(expr->token, "unsupported integer bit width");
  }

  ResolvedLit resolvedLit;

  switch (bits) {
  case 32:
    resolvedLit.type = table->getType("i32");
    break;
  case 64:
    resolvedLit.type = table->getType("i64");
    break;
  case 128:
    resolvedLit.type = table->getType("i128");
    break;
  default:
    Error::internal(expr->token, "invalid integer literal bit width");
  }

  resolvedLit.value = IntPayload(llvm::APInt(bits, llvm::StringRef(s), 10));
  return resolvedLit;
}

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
    Error::diagnostic(expr->token, "invalid floating-point literal");
  }

  auto parseStatus = *parseResult;

  if (parseStatus & llvm::APFloat::opInvalidOp) {
    Error::diagnostic(expr->token, "invalid floating-point literal");
  }
  if (parseStatus & llvm::APFloat::opOverflow) {
    Error::diagnostic(expr->token, "unsupported floating-point literal range");
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
    resolvedLit.type = table->getType(cand.name);
    resolvedLit.value = FloatPayload(convertFloatTo(base, cand.sem));
    return resolvedLit;
  }

  Error::diagnostic(expr->token, "unsupported floating-point literal range");
}
