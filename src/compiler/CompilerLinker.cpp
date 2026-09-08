#include "hrd/compiler/CompilerLinker.h"

#include <iostream>

#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/Program.h>

bool CompilerLinker::link(const LinkInput &input) {

  auto clang = llvm::sys::findProgramByName("clang++");

  if (!clang) {
    std::cerr << "error: clang++ was not found\n";
    return false;
  }

  std::error_code ec;

  const auto parent = input.output.parent_path();

  if (!parent.empty()) {
    std::filesystem::create_directories(parent, ec);

    if (ec) {
      std::cerr << "error: failed to create output directory\n"
                << "path: " << parent << '\n';
      return false;
    }
  }

  std::vector<std::string> storage;

  storage.push_back(*clang);
  storage.push_back(input.input.string());
  storage.push_back(input.runtime.string());

  for (const auto &library : input.libraries) {
    storage.push_back(library.string());
  }
  for (const auto &natvie : input.natives) {
    storage.push_back(natvie.string());
  }

  storage.push_back("-o");
  storage.push_back(input.output.string());

  llvm::SmallVector<llvm::StringRef, 16> args;
  args.reserve(storage.size());

  for (const auto &arg : storage) {
    args.push_back(arg);
  }

  const int result = llvm::sys::ExecuteAndWait(*clang, args);

  if (result != 0) {
    std::cerr << "error: linker failed\n";
    return false;
  }

  return true;
}