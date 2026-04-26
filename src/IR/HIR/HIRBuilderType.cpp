#include "IR/HIR/HIRBuilder.h"
#include "IR/HIR/HIRType.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include "enums/StorageKind.h"
#include "util/Error.h"
#include <cassert>
#include <memory>
#include <utility>

HIRType *HIRBuilder::lowerType(TypeSymbol *symbol) {
  HIRType *type = getOrCreateType(symbol);

  if (type == nullptr) {
    Error::internal("fail to get hir type");
  }
  return type;
}

HIRType *HIRBuilder::getOrCreateType(TypeSymbol *symbol) {
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
        hirType = make_unique<HIRHandleType>(
            lowerEntityType(handle->args[0]),
            dynamic_cast<HandleSymbol *>(handle->origin)->storage);
      } else {
        Error::internal("fail to cast handle symbol");
      }
      break;

    case TypeSymbol::TypeKind::RESULT:
      if (auto result = dynamic_cast<GenericSymbol *>(symbol)) {
        auto error = lowerType(result->args[1]);
        if (error->kind != HIRTypeKind::Error) {
          Error::internal("lowered type type is not error");
        }
        hirType = make_unique<HIRResultType>(
            lowerType(result->args[0]), dynamic_cast<HIRErrorType *>(error));
      } else {
        Error::internal("fail to cast result symbol");
      }
      break;

    case TypeSymbol::TypeKind::OPTION:
      if (auto option = dynamic_cast<GenericSymbol *>(symbol)) {
        hirType = make_unique<HIROptionType>(lowerType(option->args[0]));
      } else {
        Error::internal("fail to cast option symbol");
      }
      break;

    case TypeSymbol::TypeKind::ERROR:
      hirType = make_unique<HIRErrorType>();
      break;
    case TypeSymbol::TypeKind::ARRAY:
      if (auto array = dynamic_cast<ArrayTypeSymbol *>(symbol)) {
        hirType = make_unique<HIRArrayType>(lowerType(array->baseType),
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

    type = hirType.get();
    program->typeCache.emplace(symbol, type);
    source->types.push_back(std::move(hirType));
    return type;
  }
}

HIREntityType *HIRBuilder::lowerEntityType(TypeSymbol *symbol) {
  auto it = program->typeCache.find(symbol);
  if (it == program->typeCache.end()) {
    Error::internal("fail to find type : " + symbol->name);
  }
  auto type = it->second;
  if (auto entity = dynamic_cast<HIREntityType *>(type)) {
    return entity;
  } else {
    Error::internal("not entity type");
  }
}

HIRHandleType *HIRBuilder::getOrCreateHandleType(HIREntityType *entity,
                                                 StorageKind storage) {
  auto it = program->handleCache.find(entity);
  HIRHandleType *result = nullptr;
  if (it == program->handleCache.end()) {
    unique_ptr<HIRHandleType> handle =
        make_unique<HIRHandleType>(entity, storage);
    result = handle.get();
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

HIRObserverType *HIRBuilder::getOrCreateObserverType(HIREntityType *entity,
                                                     StorageKind kind) {
  auto it = program->observerCache.find(entity);
  HIRObserverType *result = nullptr;
  if (it == program->observerCache.end()) {
    unique_ptr<HIRObserverType> observer =
        make_unique<HIRObserverType>(entity, kind);
    result = observer.get();
    program->observerCache.emplace(entity, result);
    source->observers.push_back(std::move(observer));
  } else {
    result = it->second;
  }

  if (result == nullptr) {
    Error::internal("fail to get or create observer");
  }

  return result;
}