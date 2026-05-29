#pragma once

#include "IR/HIR/HIRDecl.h"
#include "IR/HIR/HIRProgram.h"
namespace HIRHelper {
void linkRoot(HIRProgram *program);
void linkSecondPass(HIRProgram *program);
HIRType *lowerType(HIRProgram *program, HIRSource *source, TypeSymbol *symbol);
HIRType *getOrCreateType(HIRProgram *program, HIRSource *source,
                         TypeSymbol *symbol);
HIREntityType *lowerEntityType(HIRProgram *program, HIRSource *source,
                               TypeSymbol *symbol);
HIRHandleType *getOrCreateHandleType(HIRProgram *program, HIRSource *source,
                                     HIREntityType *entity,
                                     StorageKind storage);
HIREnumVariant *lowerEnumVariant(HIRProgram *program, HIRTypeDecl *type,
                                 EnumDecl::Variant *v);
} // namespace HIRHelper