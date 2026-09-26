#pragma once

#include "hrd/IR/MIR/MIRExpr.h"
#include "hrd/SemanticAnalyzer/ResolvedLit.h"
#include "hrd/SemanticAnalyzer/symbol/GenericOwner.h"
#include "hrd/SemanticAnalyzer/symbol/MethodSymbol.h"
#include "hrd/SemanticAnalyzer/symbol/TypeSymbol.h"
#include <cstddef>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Metadata.h>

#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <utility>
#include <vector>

using BlockID = uint64_t;
using BlockMap = unordered_map<BlockID, llvm::BasicBlock *>;
using localMap = unordered_map<ValueSymbol *, llvm::AllocaInst *>;
using ParamMap = unordered_map<ValueSymbol *, llvm::Value *>;

struct Cleanup {
  llvm::Value *addr = nullptr;
  TypeSymbol *type = nullptr;
};
struct FuncContext {
  llvm::Function *func;
  BlockMap blocks;
  localMap locals;
  ParamMap params;
  llvm::Value *self = nullptr;
  TypeSymbol *selfType = nullptr;
  std::vector<Cleanup> cleanupStack;
  std::unordered_set<llvm::Value *> canceledCleanups;
  unordered_map<GenericParamSymbol *, TypeSymbol *> *substitution = nullptr;
};

struct GenericMethodKey {
  TypeSymbol *owner;
  MethodSymbol *method;
  std::vector<TypeSymbol *> genericArgs;
  GenericMethodKey(TypeSymbol *o, MethodSymbol *m,
                   std::vector<TypeSymbol *> g = std::vector<TypeSymbol *>())
      : owner(o), method(m), genericArgs(std::move(g)) {}
  bool operator==(const GenericMethodKey &other) const noexcept {
    auto flag = true;
    if (other.genericArgs.size() != genericArgs.size()) {
      flag = false;
    } else {
      for (size_t i = 0; i < genericArgs.size(); ++i) {
        if (genericArgs[i] != other.genericArgs[i]) {
          flag = false;
          break;
        }
      }
    }

    return owner == other.owner && method == other.method && flag;
  }
  std::string toString() const {
    std::string result;

    result += "owner=";
    result += owner != nullptr ? owner->name : "<null>";

    result += ", method=";
    result += method != nullptr ? method->name : "<null>";

    result += ", genericArgs=[";

    for (size_t i = 0; i < genericArgs.size(); ++i) {
      if (i != 0) {
        result += ", ";
      }

      auto *arg = genericArgs[i];

      if (arg == nullptr) {
        result += "<null>";
      } else {
        result += arg->name;
      }
    }

    result += "]";

    return result;
  }
};

struct GenericMethodKeyHash {
  size_t operator()(const GenericMethodKey &key) const noexcept {
    size_t hash = std::hash<TypeSymbol *>{}(key.owner);

    auto combine = [&](size_t value) {
      hash ^= value + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    };

    combine(std::hash<MethodSymbol *>{}(key.method));

    for (auto *arg : key.genericArgs) {
      combine(std::hash<TypeSymbol *>{}(arg));
    }

    return hash;
  }
};

struct LoweredValue {
  llvm::Value *value = nullptr; // 실제 SSA value
  llvm::Value *addr = nullptr;  // cleanup/release 가능한 주소
  MIRValueCategory category = MIRValueCategory::Plain;
};

struct LoweredPlace {
  llvm::Value *dst = nullptr;
  TypeSymbol *type = nullptr;
};

struct MethodContext {
  bool isGenericDecl = false;
  GenericMethodKey *key;
  unordered_map<GenericParamSymbol *, TypeSymbol *> *substitution = nullptr;
};