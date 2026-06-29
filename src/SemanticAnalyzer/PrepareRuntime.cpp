#include "hrd/SemanticAnalyzer.h"
#include "hrd/SemanticAnalyzer/SymbolTable.h"
#include "hrd/SemanticAnalyzer/symbol/RuntimeSymbol.h"
#include <memory>
#include <utility>
#include <vector>

void SemanticAnalyzer::addRuntime(std::string ns, std::string name,
                                  std::string llvmName, TypeSymbol *returnType,
                                  std::vector<TypeSymbol *> params) {
  auto runtime = std::make_unique<RuntimeSymbol>();
  runtime->namespaceName = ns;
  runtime->name = name;
  runtime->llvmName = std::move(llvmName);
  runtime->returnType = returnType;
  runtime->params = std::move(params);

  RuntimeSymbol *raw = runtime.get();

  auto [nsIt, _] = symbolTable.runtimeMap.try_emplace(runtime->namespaceName,
                                                      runtime->namespaceName);

  auto &overloads = nsIt->second.functions[runtime->name];

  runtimes.push_back(std::move(runtime));
  overloads.push_back(raw);
}

void SemanticAnalyzer::prepareRuntime() {
  addRuntime("log", "info", "hrd_log_info_s8", symbolTable.getType("void"),
             {symbolTable.getBuilt("s8")});
}