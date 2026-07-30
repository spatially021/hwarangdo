#include "hrd/SemanticAnalyzer/Resolver.h"
#include "hrd/Recover/ResolverRecover.h"
#include "hrd/SemanticAnalyzer/SymbolTable.h"
#include "hrd/compiler/CompilerContexts.h"

Resolver::Resolver(ResolverContext &ctx)
    : table(ctx.table), engine(ctx.engine), recover(*this) {}
