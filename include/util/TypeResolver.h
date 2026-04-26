#include "AST/ASTNode.h"
#include "SemanticAnalyzer/ResolvedLit.h"
#include "SemanticAnalyzer/SymbolTable.h"
namespace TypeResolver {
void resolveTypeNode(TypeNode *type, SymbolTable *table);
llvm::APInt resolveFixedArraySize(Expr *expr, SymbolTable *table);
ResolvedLit resolveLitInt(LiteralExpr *expr, SymbolTable *table);
} // namespace TypeResolver