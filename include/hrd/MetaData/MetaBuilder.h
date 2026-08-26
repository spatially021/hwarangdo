#pragma once

#include "hrd/MetaData/MetaData.h"
#include "hrd/SemanticAnalyzer/SymbolTable/SymbolTable.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/ValueSymbol.h"

class MetaBuilder {
public:
  MetaBuilder(SymbolTable &table);
  ModuleMeta build();

private:
  SymbolTable &table;
  TypeMeta buildType(const TypeSymbol *type);
  TraitMeta buildTrait(const TypeSymbol *trait);

  FieldMeta buildField(const ValueSymbol *field);
  MethodMeta buildMethod(const MethodSymbol *method);
  ParamMeta buildParam(const Param *param);
  EnumVariantMeta buildVariant(const EnumVariantSymbol *variant);

  TypeRef buildTypeRef(TypeSymbol *type);
  DefaultValueMeta buildDefaultValue(Expr *expr);
};