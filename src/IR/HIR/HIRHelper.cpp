
#include "hrd/IR/HIR/HIRHelper.h"
#include "hrd/AST/Decl.h"
#include "hrd/IR/HIR/HIRDecl.h"
#include "hrd/IR/HIR/HIRProgram.h"
#include "hrd/IR/HIR/HIRType.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/util/Error.h"
#include <utility>

void HIRHelper::linkSecondPass(HIRProgram *program) {
  for (auto &s : program->sources) {
    for (auto &d : s->source->decls) {
      if (auto *c = dynamic_cast<ClassDecl *>(d.get())) {
        auto it = program->typeDeclMap.find(c->symbol);
        if (it == program->typeDeclMap.end()) {
          Error::internal(d->span, "fail to get type");
        }
        if (c->baseClass.has_value()) {
          auto bIt = program->typeDeclMap.find(c->symbol->base);
          if (bIt == program->typeDeclMap.end()) {
            Error::internal(c->span, "fail to get baseType");
          }
          it->second->base = bIt->second;
        }
        for (auto &f : it->second->fieldMap) {
          auto field = f.second;
          field->type =
              HIRHelper::lowerType(program, s.get(), field->symbol->typeSymbol);
        }
      }

      if (auto st = dynamic_cast<StructDecl *>(d.get())) {
        auto it = program->typeDeclMap.find(st->symbol);
        if (it == program->typeDeclMap.end()) {
          Error::internal(st->span, "fail to get struct type");
        }
        for (auto &f : it->second->fieldMap) {
          auto field = f.second;
          field->type =
              HIRHelper::lowerType(program, s.get(), field->symbol->typeSymbol);
        }
      }

      if (auto e = dynamic_cast<EnumDecl *>(d.get())) {
        auto it = program->typeDeclMap.find(e->symbol);
        if (it == program->typeDeclMap.end()) {
          Error::internal(e->span, "fail to get enum type");
        }
        for (auto &v : e->variants) {
          HIRHelper::lowerEnumVariant(program, it->second, v.get());
        }
      }
    }
  }
}

HIRType *HIRHelper::lowerType(HIRProgram *program, HIRSource *source,
                              TypeSymbol *symbol) {
  HIRType *type = HIRHelper::getOrCreateType(program, source, symbol);

  if (type == nullptr) {
    Error::internal("fail to get hir type");
  }
  return type;
}

HIRType *HIRHelper::getOrCreateType(HIRProgram *program, HIRSource *source,
                                    TypeSymbol *symbol) {
  if (symbol == nullptr) {
    Error::internal("Typesymbol is nullptr");
  }
  auto it = program->typeCache.find(symbol);

  if (it != program->typeCache.end()) {
    return it->second;
  } else {
    unique_ptr<HIRType> hirType;
    HIRType *type = nullptr;
    const string &name = symbol->name;
    switch (symbol->kind) {

    case TypeSymbol::TypeKind::CLASS:
      hirType = make_unique<HIREntityType>(name, symbol);
      break;

    case TypeSymbol::TypeKind::ENUM:
      hirType = make_unique<HIREnumType>(name, symbol);
      break;

    case TypeSymbol::TypeKind::STRUCT:
      hirType = make_unique<HIRStructType>(name, symbol);
      break;
    case TypeSymbol::TypeKind::VOID:
      hirType = make_unique<HIRVoidType>();
      break;

    case TypeSymbol::TypeKind::HANDLE:
      if (auto handle = dynamic_cast<GenericSymbol *>(symbol)) {
        /*   hirType = make_unique<HIRHandleType>(
              HIRHelper::lowerEntityType(program, handle->args[0]),
              dynamic_cast<HandleSymbol *>(handle->origin)->storage);
         */
        auto hType = getOrCreateHandleType(
            program, source,
            HIRHelper::lowerEntityType(program, source, handle->args[0]),
            symbol, dynamic_cast<HandleSymbol *>(handle->origin)->storage);
        program->typeCache.emplace(symbol, hType);
        return hType;
      } else {
        Error::internal("fail to cast handle symbol");
      }
      break;

    case TypeSymbol::TypeKind::RESULT:
      if (auto result = dynamic_cast<GenericSymbol *>(symbol)) {
        auto error = HIRHelper::lowerType(program, source, result->args[1]);
        if (error->kind != HIRTypeKind::Error) {
          Error::internal("lowered type type is not error");
        }
        hirType = make_unique<HIRResultType>(
            HIRHelper::lowerType(program, source, result->args[0]),
            dynamic_cast<HIRErrorType *>(error));
      } else {
        Error::internal("fail to cast result symbol");
      }
      break;

    case TypeSymbol::TypeKind::OPTION:
      if (auto option = dynamic_cast<GenericSymbol *>(symbol)) {
        hirType = make_unique<HIROptionType>(
            HIRHelper::lowerType(program, source, option->args[0]));
      } else {
        Error::internal("fail to cast option symbol");
      }
      break;

    case TypeSymbol::TypeKind::ERROR:
      hirType = make_unique<HIRErrorType>();
      break;
    case TypeSymbol::TypeKind::ARRAY:
      if (auto array = dynamic_cast<ArrayTypeSymbol *>(symbol)) {
        hirType = make_unique<HIRArrayType>(
            HIRHelper::lowerType(program, source, array->baseType),
            array->sizeValue);
      } else {
        Error::internal("illegal symbol kind");
      }
      break;
    default:
      Error::internal("illegal type symbol kind");
    }

    if (hirType == nullptr) {
      Error::internal("fail to make ptr");
    }
    hirType->typeSymbol = symbol;
    type = hirType.get();
    program->typeCache.emplace(symbol, type);
    source->types.push_back(std::move(hirType));
    return type;
  }
}
HIREntityType *HIRHelper::lowerEntityType(HIRProgram *program,
                                          HIRSource *source,
                                          TypeSymbol *symbol) {
  auto *type = HIRHelper::lowerType(program, source, symbol);

  if (auto *entity = dynamic_cast<HIREntityType *>(type)) {
    return entity;
  }

  Error::internal("not entity type");
}

HIRHandleType *HIRHelper::getOrCreateHandleType(HIRProgram *program,
                                                HIRSource *source,
                                                HIREntityType *entity,
                                                TypeSymbol *type,
                                                StorageKind storage) {
  auto it = program->handleCache.find(entity);
  HIRHandleType *result = nullptr;
  if (it == program->handleCache.end()) {
    unique_ptr<HIRHandleType> handle =
        make_unique<HIRHandleType>(entity, storage);
    result = handle.get();
    result->storage = storage;
    result->entityType = entity;
    result->typeSymbol = type;
    program->handleCache.emplace(entity, result);
    source->handles.push_back(std::move(handle));

  } else {
    result = it->second;
  }

  if (result == nullptr) {
    Error::internal("fail to get or create handle");
  }

  return result;
}

HIREnumVariant *HIRHelper::lowerEnumVariant(HIRProgram *program,
                                            HIRTypeDecl *type,
                                            EnumDecl::Variant *v) {
  unique_ptr<HIREnumVariant> variant = make_unique<HIREnumVariant>();
  variant->id = type->nextFieldId++;
  variant->name = v->name;
  variant->kind =
      (v->payload ? HIREnumVariantKind::Payload : HIREnumVariantKind::Unit);
  variant->symbol = v->symbol;
  variant->owner = type;
  if (v->payload) {
    auto it = program->typeCache.find(v->payload.value()->resolved);
    if (it == program->typeCache.end()) {
      Error::internal("fail to find type in payload");
    }
    variant->payloadType = it->second;
  }

  auto raw = variant.get();

  type->enumVariants.push_back(std::move(variant));
  program->variantMap.emplace(raw->symbol, raw);
  return raw;
}
