#include "hrd/Imported/ImportedSymbolBuilder.h"
#include "hrd/Color.h"
#include "hrd/compiler/CompilerContexts.h"

#include <stdexcept>
#include <utility>

#ifndef NDEBUG
#define HRD_DEBUG 1
#else
#define HRD_DEBUG 0
#endif
#if HRD_DEBUG
#include <iostream>
#endif
ImportedSymbolBuilder::ImportedSymbolBuilder(ImportedContext &ctx)
    : meta(ctx.meta), module(ctx.module), table(ctx.table), ast(ctx.imported),
      scope(ctx.scope), summary(ctx.summary) {}

ImportedSymbolBuilder::~ImportedSymbolBuilder() = default;

void ImportedSymbolBuilder::run() {
  try {
    build();
  } catch (runtime_error &e) {
#if HRD_DEBUG
    cout << Color::RED << "error occur while import build\n";
#endif
    throw e;
  }
  try {
    link();
  } catch (runtime_error &e) {
#if HRD_DEBUG
    cout << Color::RED << "error occur while import link\n";
#endif
    throw e;
  }
  try {
    resolve();
  } catch (runtime_error &e) {
#if HRD_DEBUG
    cout << Color::RED << "error occur while import resolve\n";
#endif
    throw e;
  }

  try {
    load();
  } catch (runtime_error &e) {
#if HRD_DEBUG
    cout << Color::RED << "error occur while import load\n";
#endif
    throw e;
  }
}

void ImportedSymbolBuilder::build() {
  for (auto &type : meta.types) {
    currnet = &type;
    buildType(type);
  }
  for (auto &trait : meta.traits) {
    currnet = nullptr;
    buildTrait(trait);
  }
}

void ImportedSymbolBuilder::link() {
  for (auto &type : meta.types) {
    currnet = &type;
    linkType(type);
  }

  for (auto &trait : meta.traits) {
    currnet = nullptr;
    linkTrait(trait);
  }
}

void ImportedSymbolBuilder::resolve() {
  for (auto &type : meta.types) {
    currnet = &type;
    auto file = getFile(type.path);
    auto symbol = getTypeSymbol(file, type.name);

    for (auto &m : type.methods) {
      resolveMethod(m, symbol);
    }
  }

  for (auto &type : meta.traits) {
    auto file = getFile(type.path);
    auto symbol = getTypeSymbol(file, type.name);

    for (auto &m : type.methods) {
      resolveMethod(m, symbol);
    }
  }
}

void ImportedSymbolBuilder::load() {
  for (auto &type : meta.types) {
    loadType(type.path, type.name);
  }

  for (auto &trait : meta.traits) {
    loadType(trait.path, trait.name);
  }
}

void ImportedSymbolBuilder::loadType(const SourcePath &path,
                                     const string &name) {
  auto *file = table.registry.getFile(module, path);

  if (file == nullptr) {
    auto *raw = getFile(path);
    table.registry.addFile(module, path, raw);
    file = raw;
  }

  table.registry.setCurrentFile(file);

  auto &map = getTypeMap(file);
  auto it = map.find(name);

  if (it == map.end()) {
    Error::internal("fail to get imported type: " + name);
  }
  {
#if HRD_DEBUG
    auto type = it->second.get();
    type->name = type->name;
#endif
  }
  table.registry.addType(std::move(it->second));
}