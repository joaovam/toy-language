#include"AST.h"
#include"Lexer.cpp"

std::unique_ptr<ExprAST> LogError(const char *str){
    fprintf(stderr, "Error: %s\n", str);
    return nullptr;
}

std::unique_ptr<PrototypeAST> LogErrorP(const char *str){
    LogError(str);
    return nullptr;
}