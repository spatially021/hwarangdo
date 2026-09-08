#include "hrd/AST/Expr.h"
#include "hrd/SemanticAnalyzer/Resolver.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/diagnostic/Diagnostic.h"

void Resolver::visit(ArrayAccessExpr *expr) {
  expr->object->accept(this);
  expr->index->accept(this);
  if (!isa<IntType>(expr->index->resolvedType)) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S048);
    dia.labels = {{expr->index->span,
                   "array index has type '" + expr->index->resolvedType->name +
                       "', expected an integer type",
                   true}};
    engine.emit(dia);
    recover.recover();
  }
  if (auto arr = dynamic_cast<ArrayTypeSymbol *>(expr->object->resolvedType)) {

    expr->resolvedType = arr->baseType;
  } else {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S049);
    dia.labels = {
        {expr->index->span,
         "type '" + expr->object->resolvedType->name + "' cannot be indexed",
         true}};
    engine.emit(dia);
    recover.recover();
  }
}

void Resolver::visit(ThisExpr *expr) {
  assert(currentType);
  if (currentType->kind != TypeKind::CLASS) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S050);
    dia.labels = {
        {expr->span, "'this' is not available in this context", true}};
    dia.helps = {{"use 'this' only inside a class method"}};
    engine.emit(dia);
    recover.recover();
  }
  expr->resolved = currentType;
  expr->resolvedType = currentType;
}
void Resolver::visit(SuperExpr *expr) {
  assert(currentType);
  auto obj = dyn_cast<ObjectType>(currentType);
  if (currentType->kind != TypeKind::CLASS || obj == nullptr) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S051);
    dia.labels = {
        {expr->span, "'super' is not available in this context", true}};
    dia.helps = {{"use 'super' only inside a class method"}};
    engine.emit(dia);
    recover.recover();
  }

  if (obj->base == nullptr) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S052);
    dia.labels = {{expr->span, "'super' requires a parent class", true}};
    dia.helps = {{"remove 'super' or inherit from another class"}};
    engine.emit(dia);
    recover.recover();
  }
  expr->resolved = obj->base;
  expr->resolvedType = obj->base;
}

void Resolver::visit(RootExpr *expr) {
  expr->resolved = table.registry.getRoot();
  expr->resolvedType = table.registry.getRoot();
}
void Resolver::visit(SelfExpr *expr) {
  assert(currentType);
  if (currentType->kind != TypeKind::STRUCT) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_S053);
    dia.labels = {
        {expr->span, "'self' is not available in this context", true}};
    dia.helps = {{"use 'self' only inside an impl method"}};
    engine.emit(dia);
    recover.recover();
  }
  expr->resolved = currentType;
  expr->resolvedType = currentType;
}