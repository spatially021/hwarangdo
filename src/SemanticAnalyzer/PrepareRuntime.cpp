#include "hrd/SemanticAnalyzer.h"
#include "hrd/SemanticAnalyzer/SymbolTable/SymbolTable.h"
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

  table.registry.addRuntime(std::move(runtime));
}
void SemanticAnalyzer::addLog() {
  addRuntime("log", "info", "hrd_log_info_s8", table.getType("void"),
             {table.registry.getBuilt("s8")});
  addRuntime("log", "info", "hrd_log_info_i32", table.getType("void"),
             {table.registry.getBuilt("i32")});
  addRuntime("log", "info", "hrd_log_info_u32", table.getType("void"),
             {table.registry.getBuilt("u32")});
  addRuntime("log", "info", "hrd_log_info_f32", table.getType("void"),
             {table.registry.getBuilt("f32")});
  addRuntime("log", "info", "hrd_log_info_bool", table.getType("void"),
             {table.registry.getBuilt("bool")});
  addRuntime("log", "info", "hrd_log_info_c8", table.getType("void"),
             {table.registry.getBuilt("c8")});

  addRuntime("log", "info", "hrd_log_info_s16", table.getType("void"),
             {table.registry.getBuilt("s16")});

  addRuntime("log", "info", "hrd_log_info_s32", table.getType("void"),
             {table.registry.getBuilt("s32")});
}

void SemanticAnalyzer::prepareRuntime() { addLog(); }