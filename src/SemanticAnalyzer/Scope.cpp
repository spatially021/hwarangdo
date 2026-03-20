#include "SemanticAnalyzer/Scope.h"

Scope::Scope() = default;
Scope::~Scope() = default;

BuiltInScope::BuiltInScope() { scopeKind = Scope::ScopeKind::BUILTIN; }
BuiltInScope::~BuiltInScope() = default;