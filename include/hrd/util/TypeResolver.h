#pragma once

#include "hrd/AST/ASTNode.h"
#include "hrd/Recover/Recover.h"
#include "hrd/SemanticAnalyzer/ResolvedLit.h"
#include "hrd/SemanticAnalyzer/SymbolTable/SymbolTable.h"
#include "hrd/diagnostic/DiagnosticEngine.h"

struct TypeResolverContext {
  DiagnosticEngine &engine;
  SymbolTable &table;
  Recover &recover;
};

namespace TypeResolver {
void resolveTypeNode(TypeNode *type, TypeResolverContext &context);
llvm::APInt resolveFixedArraySize(Expr *expr, TypeResolverContext &context);
ResolvedLit resolveLitInt(LiteralExpr *expr, TypeResolverContext &context);
} // namespace TypeResolver