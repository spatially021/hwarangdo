#pragma once
#include "Types.h"
#include <string>
#include <unordered_map>
#include <vector>

/*
symbol table for variables and functions
*/
struct Symbol {
    std::string name;
    TypeInfo type;
    bool moved = false;
};

struct SymTab {
    void enter();
    void leave();
    Symbol* find(const std::string& name);
    Symbol& declare(const Symbol& sym);
private:
    std::vector<std::unordered_map<std::string, Symbol>> scopes;
};
