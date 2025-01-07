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

public:
    PrototypeAST(const std::string& name, std::vector<std::string> args);
    const std::string& getName() const;
    Function* codegen();
};

class FunctionAST {
    std::unique_ptr<PrototypeAST> proto;
    std::unique_ptr<ExprAST> body;

public:
    FunctionAST(std::unique_ptr<PrototypeAST> proto, std::unique_ptr<ExprAST> body);
    Function* codegen();
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

extern ExitOnError ExitOnErr;

#endif
