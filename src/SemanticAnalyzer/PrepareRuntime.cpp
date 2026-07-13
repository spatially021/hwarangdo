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
void SemanticAnalyzer::addLog() {
  addRuntime("log", "info", "hrd_log_info_s8", symbolTable.getType("void"),
             {symbolTable.getBuilt("s8")});
  addRuntime("log", "info", "hrd_log_info_i32", symbolTable.getType("void"),
             {symbolTable.getBuilt("i32")});
  addRuntime("log", "info", "hrd_log_info_u32", symbolTable.getType("void"),
             {symbolTable.getBuilt("u32")});
  addRuntime("log", "info", "hrd_log_info_f32", symbolTable.getType("void"),
             {symbolTable.getBuilt("f32")});
  addRuntime("log", "info", "hrd_log_info_bool", symbolTable.getType("void"),
             {symbolTable.getBuilt("bool")});
  addRuntime("log", "info", "hrd_log_info_c8", symbolTable.getType("void"),
             {symbolTable.getBuilt("c8")});
}

void SemanticAnalyzer::prepareRuntime() { addLog(); }