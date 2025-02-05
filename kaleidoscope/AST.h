#ifndef AST_H
#define AST_H

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include "KaleidoscopeJIT/KaleidoscopeJIT.h"

#include <string>
#include <memory>
#include <vector>
#include<map>


using namespace llvm;
using namespace orc;

class ExprAST {
public:
    virtual ~ExprAST() = default;
    virtual Value* codegen() = 0;
};

class NumberExprAST : public ExprAST {
    double val;

public:
    NumberExprAST(double val);
    Value* codegen() override;
};

class VariableExprAST : public ExprAST {
    std::string name;

public:
    VariableExprAST(const std::string& name);
    Value* codegen() override;
};

class BinaryExprAST : public ExprAST {
    char op;
    std::unique_ptr<ExprAST> LHS, RHS;

public:
    BinaryExprAST(char op, std::unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS);
    Value* codegen() override;
};

class CallExprAST : public ExprAST {
    std::string callee;
    std::vector<std::unique_ptr<ExprAST>> args;

public:
    CallExprAST(const std::string& callee, std::vector<std::unique_ptr<ExprAST>> args);
    Value* codegen() override;
};

class PrototypeAST {
    std::string name;
    std::vector<std::string> args;
    bool isOperator;
    unsigned precedence;

public:
    PrototypeAST(const std::string& name, std::vector<std::string> args,
    bool isOperator = false, unsigned prec = 0);
    const std::string& getName() const;
    Function* codegen();

    bool isUnaryOp() const{ return isOperator && args.size() == 1;}
    bool isBinaryOp() const{ return isOperator && args.size() == 2;}
    char getOperatorName()const{
        assert(isUnaryOp() || isBinaryOp());
        return name[name.size() - 1];
    }
    unsigned getBinaryPrecedence() const { return precedence;}
};

class FunctionAST {
    std::unique_ptr<PrototypeAST> proto;
    std::unique_ptr<ExprAST> body;

public:
    FunctionAST(std::unique_ptr<PrototypeAST> proto, std::unique_ptr<ExprAST> body);
    Function* codegen();
};

class IfExprAST : public ExprAST{
    std::unique_ptr<ExprAST> Cond, Then, Else;
public:
    IfExprAST(std::unique_ptr<ExprAST>Cond, std::unique_ptr<ExprAST> Then, 
    std::unique_ptr<ExprAST> Else) 
    : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}
    Value *codegen() override;
};

class ForExprAST : public ExprAST{
    std::string varName;
    std::unique_ptr<ExprAST> start, end, step, body;
public:
    ForExprAST(const std::string& varName, std::unique_ptr<ExprAST> start, std::unique_ptr<ExprAST> end, 
    std::unique_ptr<ExprAST> step, std::unique_ptr<ExprAST> body) : varName(varName), start(std::move(start)), end(std::move(end)),
    step(std::move(step)), body(std::move(body)) {}
    Value *codegen() override;
};


extern std::unique_ptr<LLVMContext> context;
extern std::unique_ptr<IRBuilder<>> builder;
extern std::unique_ptr<Module> module;
extern std::map<std::string, Value*> namedValues;
extern std::unique_ptr<KaleidoscopeJIT> JIT;
extern std::unique_ptr<FunctionPassManager> FPM;
extern std::unique_ptr<LoopAnalysisManager> LAM;
extern std::unique_ptr<FunctionAnalysisManager> FAM;
extern std::unique_ptr<CGSCCAnalysisManager> CGAM;
extern std::unique_ptr<ModuleAnalysisManager> MAM;
extern std::unique_ptr<PassInstrumentationCallbacks> PIC;
extern std::unique_ptr<StandardInstrumentations> SI;
extern std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
extern std::map<char, int> BinopPrecedence;

extern ExitOnError ExitOnErr;

#endif
