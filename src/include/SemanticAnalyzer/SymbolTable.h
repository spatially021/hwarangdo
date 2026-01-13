#pragma once

#include "Symbol.h"
#include <memory>
#include <vector>
class SymbolTable{
    using scopePtr=shared_ptr<Scope>;

public:
    SymbolTable();
    vector<unique_ptr<ImplSymbol>> impls;
    bool add(unique_ptr<Symbol> symbol);
    
    void enter();
    void exit();

    Symbol resolve(const string & name);

    TypeSymbol * getType(const string & name);
    ValueSymbol *getValue(const string & name);

    Scope * getCurrent();
    
    void addImpl(unique_ptr<ImplSymbol>);

private:
    unique_ptr<Scope> topLevel;
    Scope* current;

    bool addValue(unique_ptr<ValueSymbol> symbol);
    bool addType(unique_ptr<TypeSymbol> symbol);
};