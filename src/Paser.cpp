#include "hgm/Parser.h"
#include <stdexcept>

Parser::Parser(const std::vector<Token>& t) : toks(t) {}

const Token& Parser::peek() const { return toks[p]; }
const Token& Parser::get() { return toks[p++]; }
bool Parser::match(TokKind k) { if (peek().kind==k){get();return true;} return false; }
const Token& Parser::expect(TokKind k,const std::string& msg){
    if(peek().kind!=k) throw std::runtime_error("parse error: "+msg);
    return get();
}

Program Parser::parseProgram(){
    Program prog;
    // TODO: implement
    return prog;
}
ClassDecl Parser::parseClass(){ return {"dummy",{}}; }
FnDecl Parser::parseFn(){ return {{"void"},"dummy",{},Block{}}; }
Param Parser::parseParam(){ return {{"int"},"x"}; }
Block Parser::parseBlock(){ return {}}; 
StmtPtr Parser::parseStmt(){ return std::make_shared<Stmt>(Stmt::SReturn{ReturnStmt{}}); }
ExprPtr Parser::parseExpr(){ return std::make_shared<Expr>(Expr::Number{"0"}); }
