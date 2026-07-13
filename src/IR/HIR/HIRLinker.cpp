#include "hrd/IR/HIR/HIRLinker.h"
#include "hrd/AST/Decl.h"
#include "hrd/IR/HIR/HIRDecl.h"
#include "hrd/IR/HIR/HIRHelper.h"
#include "hrd/IR/HIR/HIRType.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include "hrd/util/Error.h"
#include "hrd/util/Guard.h"
#include <memory>
#include <utility>

using std::unique_ptr;

unique_ptr<HIRSource> HIRLinker::link() {
  for (auto &d : source->decls) {
    if (dynamic_cast<ImplDecl *>(d.get())) {
      continue;
    }
    lowerTypeShell(d.get());
  }
  for (auto &d : source->decls) {
    if (auto *c = dynamic_cast<ClassDecl *>(d.get())) {
      auto it = program->typeDeclMap.find(c->symbol);
      if (it == program->typeDeclMap.end()) {
        Error::internal(d->span, "fail to get method's owner type");
      }
      for (auto &m : c->methods) {
        lowerMethodDeclShell(m.get(), it->second);
      }
      for (auto &f : c->fields) {
        lowerField(f.get(), it->second);
      }
    }
    if (auto *i = dynamic_cast<ImplDecl *>(d.get())) {
      auto symbol = table->getType(i->target);
      auto it = program->typeDeclMap.find(symbol);
      if (it == program->typeDeclMap.end()) {
        Error::internal(i->span, "fail to find impl target type");
      }
      for (auto &m : i->LinkedImplMethods) {
        lowerMethodDeclShell(m.get(), it->second);
      }
    }
    if (auto s = dynamic_cast<StructDecl *>(d.get())) {
      auto it = program->typeDeclMap.find(s->symbol);
      if (it == program->typeDeclMap.end()) {
        Error::internal(d->span, "fail to get method's owner type");
      }
      for (auto &f : s->fields) {
        lowerField(f.get(), it->second);
      }
      for (auto &i : s->inits) {
        lowerMethodDeclShell(i.get(), it->second);
      }
    }
    if (auto e = dynamic_cast<EnumDecl *>(d.get())) {
      auto it = program->typeDeclMap.find(e->symbol);
      if (it == program->typeDeclMap.end()) {
        Error::internal(e->span, "fail to get variant's owner type");
      }
      // TODO: lower enum field하기
    }
  }
  return std::move(hirSource);
}

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
  if (dynamic_cast<ImplDecl *>(decl)) {
    return nullptr;
  }
  Error::internal(decl->span, "unmatched decl type");
}

void HIRLinker::lowerTypeShell(Decl *decl) {

  HIRTypeDeclKind kind;
  TypeSymbol *typeSymbol = getTypeSymbolFromDecl(decl, kind);

  if (typeSymbol == nullptr) {
    if (dynamic_cast<TraitDecl *>(decl) == nullptr) {
      Error::internal(decl->span, "decl's typeSymbol is nullptr");
    }
    return;
  }

  HIRType *type = HIRHelper::lowerType(program, hirSource.get(), typeSymbol);
  if (type == nullptr) {
    Error::internal(decl->span, "lowerType returned nullptr");
  }

  auto ty = make_unique<HIRTypeDecl>(decl->span, kind, typeSymbol->name, type,
                                     typeSymbol);

  auto *raw = ty.get();
  auto [it, inserted] = program->typeDeclMap.emplace(typeSymbol, raw);
  if (!inserted) {
    Error::internal(decl->span, "duplicate HIR type shell for type");
  }

  hirSource->typeDecls.push_back(std::move(ty));
}

void HIRLinker::lowerField(VarDecl *decl, HIRTypeDecl *type) {
  unique_ptr<HIRField> field = make_unique<HIRField>();
  field->symbol = decl->symbol;
  field->name = decl->name;
  field->isMutable = decl->isMutable;
  field->id = type->nextFieldId++;
  auto raw = field.get();
  type->fieldMap.emplace(decl->symbol, raw);
  type->fields.push_back(std::move(field));
}

void HIRLinker::lowerMethodDeclShell(FuncDecl *decl, HIRTypeDecl *currentType) {
  assert(decl);
  assert(decl->methodSymbol);

  auto method = make_unique<HIRMethodDecl>(decl->span, currentType,
                                           currentType->nextMethodID++,
                                           decl->name, decl->methodSymbol);

  auto *raw = method.get();

  method->isAsync = false;
  method->methodKind = decl->methodSymbol->methodKind;
  method->returnType = HIRHelper::lowerType(program, hirSource.get(),
                                            decl->methodSymbol->returnType);
  vector<unique_ptr<HIRParam>> params;

  {
    MethodGuard _(currentMethod, raw);
    for (auto &param : decl->params) {
      params.push_back(lowerParam(param.get()));
    }
    method->setParam(std::move(params));
  }
  currentType->methods.push_back(std::move(method));
  switch (raw->methodKind) {

  case MethodKind::Normal:
    currentType->methodMap.emplace(decl->methodSymbol, raw);
    return;
  case MethodKind::Init:
    currentType->initMap.emplace(decl->methodSymbol, raw);
    return;
  case MethodKind::OnDestroy:
    currentType->onDestroy = raw;
    return;
  }
}

unique_ptr<HIRParam> HIRLinker::lowerParam(Param *decl) {
  unique_ptr<HIRParam> param = make_unique<HIRParam>();
  param->symbol = decl->symbol;
  param->name = decl->symbol->name;
  param->id = currentMethod->nextParamID++;
  param->type =
      HIRHelper::lowerType(program, hirSource.get(), decl->symbol->typeSymbol);
  return param;
}
