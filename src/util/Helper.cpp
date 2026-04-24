#include "util/Helper.h"
#include "SemanticAnalyzer/symbol/MethodSymbol.h"
std::string Helper::apIntToString(const llvm::APInt &v) {
  llvm::SmallString<32> buf;
  v.toString(buf, 10, false);
  return std::string(buf.str());
}

bool Helper::hasSameSig(const vector<MethodSymbol *> &vec,
                        MethodSymbol *symbol) {
  for (auto *m : vec) {
    if (m->paramTypes.size() != symbol->paramTypes.size()) {
      continue;
    }

    bool same = true;
    for (size_t i = 0; i < m->paramTypes.size(); ++i) {
      if (m->paramTypes[i] != symbol->paramTypes[i]) {
        same = false;
        break;
      }
    }

    if (same) {
      return true;
    }
  }

  return false;
}

bool Helper::hasSameSig(const vector<TraitSig *> &vec, TraitSig *sig) {
  for (auto *m : vec) {
    if (m->params.size() != sig->params.size()) {
      continue;
    }

    bool same = true;
    for (size_t i = 0; i < m->params.size(); ++i) {
      if (m->params[i] != sig->params[i]) {
        same = false;
        break;
      }
    }

    if (same) {
      return true;
    }
  }

  return false;
}