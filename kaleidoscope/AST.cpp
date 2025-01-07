#include "AST.h"
#include "ErrorHandler.h"

Function *getFunction(std::string name){
  if(auto *F = module->getFunction(name))
    return F;
  
  auto FI = FunctionProtos.find(name);
  if(FI != FunctionProtos.end())
    return FI->second->codegen();

  return nullptr;
}

// NumberExprAST implementation
NumberExprAST::NumberExprAST(double val) : val(val) {}

Value *NumberExprAST::codegen() {
  return ConstantFP::get(*context, APFloat(val));
}

// VariableExprAST implementation
VariableExprAST::VariableExprAST(const std::string &name) : name(name) {}

Value *VariableExprAST::codegen() {
  Value *v = namedValues[name];

  if (!v)
    LogErrorV("Unknown variable name");

  return v;
}

// BinaryExprAST implementation
BinaryExprAST::BinaryExprAST(char op, std::unique_ptr<ExprAST> LHS,
                             std::unique_ptr<ExprAST> RHS)
    : op(op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}

Value *BinaryExprAST::codegen() {
  Value *l = LHS->codegen();
  Value *r = RHS->codegen();

  if (!l || !r)
    return nullptr;

  switch (op) {
  case '+':
    return builder->CreateFAdd(l, r, "addtmp");

  case '-':
    return builder->CreateFSub(l, r, "subtmp");

  case '*':
    return builder->CreateFMul(l, r, "multmp");

  case '<':
    l = builder->CreateFCmpULT(l, r, "cmptmp");

    return builder->CreateUIToFP(l, Type::getDoubleTy(*context), "booltmp");

  default:
    return LogErrorV("Invalid binary operator");
  }
}

// CallExprAST implementation
CallExprAST::CallExprAST(const std::string &callee,
                         std::vector<std::unique_ptr<ExprAST>> args)
    : callee(callee), args(std::move(args)) {}

Value *CallExprAST::codegen() {
  Function *calleeF = getFunction(callee);
  if (!calleeF)
    return LogErrorV("Unknown function referenced");

  if (calleeF->arg_size() != args.size())
    return LogErrorV("Incorrect # arguments passed");

  std::vector<Value *> argsV;

  for (unsigned i = 0, e = args.size(); i < e; ++i) {
    argsV.push_back(args[i]->codegen());
    if (!argsV.back())
      return nullptr;
  }
  return builder->CreateCall(calleeF, argsV, "calltmp");
}

// PrototypeAST implementation
PrototypeAST::PrototypeAST(const std::string &name,
                           std::vector<std::string> args)
    : name(name), args(std::move(args)) {}

const std::string &PrototypeAST::getName() const { return name; }

Function *PrototypeAST::codegen() {

  std::vector<Type *> Doubles(args.size(), Type::getDoubleTy(*context));

  FunctionType *FT =
      FunctionType::get(Type::getDoubleTy(*context), Doubles, false);

  Function *F =
      Function::Create(FT, Function::ExternalLinkage, name, module.get());

  unsigned idX = 0;
  for (auto &arg : F->args())
    arg.setName(args[idX++]);

  return F;
}

// FunctionAST implementation
FunctionAST::FunctionAST(std::unique_ptr<PrototypeAST> proto,
                         std::unique_ptr<ExprAST> body)
    : proto(std::move(proto)), body(std::move(body)) {}

Function *FunctionAST::codegen() {
  auto &p = *proto;

  FunctionProtos[proto->getName()] = std::move(proto);

  Function *f = getFunction(p.getName());
  if(!f)
    return nullptr;

  BasicBlock *BB = BasicBlock::Create(*context, "entry", f);
  builder->SetInsertPoint(BB);

  namedValues.clear();

  for (auto &arg : f->args())
    namedValues[std::string(arg.getName())] = &arg;

  if(Value *retVal = body->codegen()) {
    builder->CreateRet(retVal);

    verifyFunction(*f);
    FPM->run(*f, *FAM);
    return f;
  }

  f->eraseFromParent();
  return nullptr;
}
