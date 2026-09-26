#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class GenericParamSymbol;
using namespace std;

class GenericOnwer {
protected:
  std::vector<std::unique_ptr<GenericParamSymbol>> genericParamOwn;
  std::vector<GenericParamSymbol *> genericParams;
  std::unordered_map<std::string, GenericParamSymbol *> genericParamMap;

public:
  vector<GenericParamSymbol *> &getGenericParams() { return genericParams; }
  unordered_map<string, GenericParamSymbol *> &getGenericParamMap() {
    return genericParamMap;
  }

  bool addGenericParam(unique_ptr<GenericParamSymbol> symbol);
  bool isGenericDecl = false;
};