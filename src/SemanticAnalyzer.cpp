#include "include/SemanticAnalyzer.h"
#include "include/AST/Stmt.h"

#include <vector>

using std::vector;

SemanticAnalyzer::SemanticAnalyzer(vector<Stmt::Ptr> s){
    ast=std::move(s);
}

void SemanticAnalyzer::analye(){

    Builder builder(&symbolTable);
    
    for(auto a:ast){
        a->accept(&builder);
    }

    Resolver resolver(&symbolTable);
    for(auto a:ast){
        a->accept(&resolver);
    }

}

void SemanticAnalyzer::error(const Token &token, const std::string &message) const {
  string m = "[line ";
  m += std::to_string(token.line);
  m += "] Error at '" + token.text + "': " + message;

  throw runtime_error(m);
}