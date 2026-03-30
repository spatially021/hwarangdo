#include "IR/HIR/HIRLinker.h"
#include "SemanticAnalyzer/symbol/TypeSymbol.h"
#include <memory>

using std::unique_ptr;

TypeSymbol *HIRLinker::getTypeSymbolFromDecl(Decl *decl,
                                             HIRTypeDeclKind &kind) {
  if (auto *c = dynamic_cast<ClassDecl *>(decl)) {
    kind = HIRTypeDeclKind::Class;
    return c->symbol;
  }
  if (auto *e = dynamic_cast<EnumDecl *>(decl)) {
    kind = HIRTypeDeclKind::Enum;
    return e->symbol;
  }
  if (auto *s = dynamic_cast<StructDecl *>(decl)) {
    kind = HIRTypeDeclKind::Struct;
    return s->symbol;
  }
  if (dynamic_cast<TraitDecl *>(decl)) {
    return nullptr;
  }

  Error::internal(decl->token, "unmatched decl type");
}

HIREntityType *HIRLinker::lowerEntityType(TypeSymbol *symbol) {
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

HIRType *HIRLinker::getOrCreateType(TypeSymbol *symbol) {
  if (symbol == nullptr) {
    Error::internal("symbol is nullptr");
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
      Error::internal("yet developed arrayType");
      // TODO: HIRArrayType 만들고 처리.
      break;

    default:
      Error::internal("illegal type symbol kind");
    }

    if (hirType == nullptr) {
      Error::internal("fail to make ptr");
    }

    type = hirType.get();
    hirSource->types.push_back(std::move(hirType));
    program->typeCache.emplace(symbol, type);
    return type;
  }
}

HIRType *HIRLinker::lowerType(TypeSymbol *symbol) {
  HIRType *type = getOrCreateType(symbol);

  if (type == nullptr) {
    Error::internal("fail to get hir type");
  }
  return type;
}

void HIRLinker::lowerTypeShell(Decl *decl) {

  HIRTypeDeclKind kind;
  TypeSymbol *typeSymbol = getTypeSymbolFromDecl(decl, kind);

  if (typeSymbol == nullptr) {
    // nullptr일 경우 trait
    return;
  }

  HIRType *type = lowerType(typeSymbol);
  if (type == nullptr) {
    Error::internal(decl->token, "lowerType returned nullptr");
  }

  auto ty = make_unique<HIRTypeDecl>(kind, typeSymbol->name, type);

  auto *raw = ty.get();
  auto [it, inserted] = program->typeDeclMap.emplace(typeSymbol, raw);
  if (!inserted) {
    Error::internal(decl->token, "duplicate HIR type shell for type");
  }

  hirSource->typeDecls.push_back(std::move(ty));
}

unique_ptr<HIRSource> HIRLinker::link() {
  for (auto &d : source->decls) {
    lowerTypeShell(d.get());
  }
  return std::move(hirSource);
}