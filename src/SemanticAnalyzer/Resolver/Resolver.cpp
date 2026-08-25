#include "hrd/SemanticAnalyzer/Resolver.h"
#include "hrd/Recover/ResolverRecover.h"
#include "hrd/compiler/CompilerContexts.h"

Resolver::Resolver(ResolverContext &ctx)
    : table(ctx.table), engine(ctx.engine), recover(*this),
      typeContext({ctx.engine, ctx.table, recover}) {}
