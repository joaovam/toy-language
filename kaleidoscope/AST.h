#ifndef AST_H
#define AST_H
#include<string>
#include<memory>
#include<vector>
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"

using namespace llvm;
class ExprAST{
    public:
    virtual ~ExprAST() = default;
    virtual Value * codegen() = 0;
};

class NumberExprAST : public ExprAST{
    double val;
    public:
    NumberExprAST(double val) : val(val){}
    Value *codegen() override;
};

class VariableExprAST : public ExprAST{
    std::string name;

    public:
    VariableExprAST(const std::string &name) : name(name){}
    Value *codegen() override;
};

class BinaryExprAST : public ExprAST{
    char op;
    std::unique_ptr<ExprAST> LHS, RHS;

    public:
    BinaryExprAST(char op, std::unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS) 
    : op(op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
    Value *codegen() override;
};

class CallExprAST : public ExprAST{
    std::string callee;
    std::vector<std::unique_ptr<ExprAST>> args;

    public:
    CallExprAST(const std::string & callee,
    std::vector<std::unique_ptr<ExprAST>> args)
    : callee(callee), args(std::move(args)) {}
    Value *codegen() override;
};

class PrototypeAST{
    std::string name;
    std::vector<std::string> args;

    public:
    PrototypeAST(const std::string &name, std::vector<std::string> args)
    : name(name), args(std::move(args)){}
    const std::string &getName() const {return name;}
    Function *codegen();
};

class FunctionAST{
    std::unique_ptr<PrototypeAST> proto;
    std::unique_ptr<ExprAST> body;

    public:
    FunctionAST(std::unique_ptr<PrototypeAST> proto,
    std::unique_ptr<ExprAST> body)
    : proto(std::move(proto)), body(std::move(body)) {}
    Function *codegen();
};
#endif