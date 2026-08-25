#pragma once

#include "hrd/AST/Visitor.h"
#include "hrd/Recover/LinkerRecover.h"
#include "hrd/SemanticAnalyzer/SymbolTable/SymbolTable.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/compiler/CompilerContexts.h"
#include "hrd/diagnostic/DiagnosticEngine.h"

class Linker : public ASTVisitor {

private:
  SymbolTable &table;
  TypeSymbol *currentType = nullptr;
  DiagnosticEngine &engine;
  LinkerRecover recover;

public:
  Linker(LinkerContext &context);
#define AST_NODE(T) void visit(T *node) override;
#include "../AST/ASTNodeList.def"
#undef AST_NODE

private:
  inline bool isDeclField(TypeSymbol *type) {
    return (type->kind == TypeSymbol::TypeKind::PRIMITIVE ||
            type->kind == TypeSymbol::TypeKind::HANDLE ||
            type->kind == TypeSymbol::TypeKind::STRUCT ||
            type->kind == TypeSymbol::TypeKind::ENUM);
  }
};