#include "hrd/AST/CaseAble.h"
#include "hrd/SemanticAnalyzer/Resolver.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SourceSpan.h"

void Resolver::checkSwitchValue(CaseAble *expr, SourceSpan &span) {

  auto *targetType = expr->value->resolvedType;

  if (isa<EnumType>(targetType->base())) {
    auto en = getEnumType(targetType);
    if (expr->usedVariants.size() != en->variants.size() && !expr->hasDefault) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S064);
      dia.labels = {
          {span, "this match does not handle every enum variant", true},
      };
      dia.notes = {
          "match expressions must handle every possible value",
      };
      dia.helps = {
          "add the missing enum cases or add a final wildcard case",
      };
      engine.emit(dia);
      recover.recover();
    }
  } else if (targetType->kind == TypeKind::PRIMITIVE) {
    if (!expr->hasDefault) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S064);
      dia.labels = {
          {span, "this match has no wildcard case", true},
      };
      dia.notes = {
          "primitive values cannot be exhaustively enumerated by case values",
      };
      dia.helps = {
          "add a final wildcard case",
      };
      engine.emit(dia);
      recover.recover();
    }
  } else {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S065);
    dia.labels = {
        {expr->value->span,
         "this expression has type '" + targetType->name + "'", true},
    };
    dia.notes = {
        "match expressions only support primitive and enum target values",
    };
    dia.helps = {
        "use a primitive or enum expression as the match target",
    };
    engine.emit(dia);
    recover.recover();
  }
}