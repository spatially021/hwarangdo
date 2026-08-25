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
      scope(ctx.scope) {}

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
}

void ImportedSymbolBuilder::link() {
  for (auto &type : meta.types) {
    currnet = &type;
    linkType(type);
  }
}

void ImportedSymbolBuilder::resolve() {
  for (auto &type : meta.types) {
    currnet = &type;
    for (auto &m : type.methods) {
      resolveMethod(m);
    }
  }
}

void ImportedSymbolBuilder::load() {
  for (auto &type : meta.types) {
    auto file = table.registry.getFile(module, type.path);
    if (file == nullptr) {
      auto raw = getFile(type.path);
      table.registry.addFile(module, type.path, raw);
      file = raw;
    }
    table.registry.setCurrentFile(file);
    auto &map = getTypeMap(file);
    auto it = map.find(type.name);
    if (it == map.end()) {
      Error::internal("fail to get type");
    }
    table.registry.addType(std::move(it->second));
  }
}