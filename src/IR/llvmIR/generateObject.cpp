#include "hrd/IR/llvmIR/llvmCodegen.h"
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>

bool llvmCodegen::emitObject(const std::filesystem::path &outputPath) {

  const std::string targetTripleStr = llvm::sys::getDefaultTargetTriple();

  const llvm::Triple targetTriple(targetTripleStr);

  llvmModule->setTargetTriple(targetTriple);

  std::string error;

  const llvm::Target *target =
      llvm::TargetRegistry::lookupTarget(targetTriple, error);

  if (target == nullptr) {
    throw std::runtime_error("failed to lookup LLVM target: " + error);
  }

  llvm::TargetOptions options;

  std::unique_ptr<llvm::TargetMachine> targetMachine(
      target->createTargetMachine(targetTriple, "generic", "", options,
                                  std::nullopt));

  if (!targetMachine) {
    throw std::runtime_error("failed to create LLVM target machine");
  }

  llvmModule->setDataLayout(targetMachine->createDataLayout());

  std::error_code ec;

  const auto parent = outputPath.parent_path();

  if (!parent.empty()) {
    std::filesystem::create_directories(parent, ec);

    if (ec) {
      throw std::runtime_error("failed to create object output directory: " +
                               ec.message());
    }
  }

  llvm::raw_fd_ostream output(outputPath.string(), ec, llvm::sys::fs::OF_None);

  if (ec) {
    throw std::runtime_error("failed to open object output: " + ec.message());
  }

  llvm::legacy::PassManager passManager;

  if (targetMachine->addPassesToEmitFile(passManager, output, nullptr,
                                         llvm::CodeGenFileType::ObjectFile)) {
    throw std::runtime_error("target machine cannot emit object file");
  }

  passManager.run(*llvmModule);
  output.flush();

  return true;
}