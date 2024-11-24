#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H
#include"AST.h"

std::unique_ptr<ExprAST> LogError(const char *str);
std::unique_ptr<PrototypeAST> LogErrorP(const char *str);
Value *LogErrorV(const char *str);
#endif