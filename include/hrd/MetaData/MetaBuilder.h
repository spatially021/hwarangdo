#pragma once

#include "hrd/InitChecker/InitSummary.h"
#include "hrd/MetaData/MetaData.h"

class SymbolTable;
class TypeSymbol;
class ValueSymbol;
class MethodSymbol;
class EnumVariantSymbol;
class Param;
class Expr;

struct MetaBuilderContext;

class MetaBuilder {
public:
  explicit MetaBuilder(MetaBuilderContext &ctx);

  ModuleMeta build();

private:
  SymbolTable &table;
  const InitSummary &summary;

  TypeMeta buildType(TypeSymbol *type);
  TraitMeta buildTrait(TypeSymbol *type);
  FieldMeta buildField(ValueSymbol *symbol);
  MethodMeta buildMethod(MethodSymbol *symbol);
  ParamMeta buildParam(Param *param);
  DefaultValueMeta buildDefaultValue(Expr *expr);
  EnumVariantMeta buildVariant(EnumVariantSymbol *symbol);
  TypeRef buildTypeRef(TypeSymbol *symbol);
};