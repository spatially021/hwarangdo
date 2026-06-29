#pragma once

#include "hrd/AST/Visitor.h"
#include "hrd/SemanticAnalyzer/SymbolTable.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"

class Linker : public ASTVisitor {

private:
  SymbolTable *table = nullptr;
  TypeSymbol *currentType = nullptr;

public:
  Linker(SymbolTable *t);
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