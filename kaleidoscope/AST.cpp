#include "AST.h"
#include "ErrorHandler.h"

static AllocaInst* CreateEntryBlockAlloca(Function * func, StringRef varName){
  IRBuilder<> tmpB(&func->getEntryBlock(), func->getEntryBlock().begin());

  return tmpB.CreateAlloca(Type::getDoubleTy(*context), nullptr, varName);
}

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
  AllocaInst *a = namedValues[name];

  if(!a)
    return LogErrorV("Unknown variable name");
  
  return builder->CreateLoad(a->getAllocatedType(), a, name.c_str());
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
    break;
  }
  Function *f = getFunction(std::string("binary") + op);
  assert(f && "binary operator not found");

  Value *ops[2] = {l, r};

  return builder->CreateCall(f, ops, "binop");
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
                           std::vector<std::string> args, bool isOperator, unsigned prec)
    : name(name), args(std::move(args)), isOperator(isOperator), precedence(prec) {}

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

  if(p.isBinaryOp())
    BinopPrecedence[p.getOperatorName()] = p.getBinaryPrecedence();

  BasicBlock *BB = BasicBlock::Create(*context, "entry", f);
  builder->SetInsertPoint(BB);

  namedValues.clear();

  for (auto &arg : f->args()){
    AllocaInst* alloca = CreateEntryBlockAlloca(f, arg.getName());
    builder->CreateStore(&arg, alloca);
    namedValues[std::string(arg.getName())] = alloca;
  }

  if(Value *retVal = body->codegen()) {
    builder->CreateRet(retVal);

    verifyFunction(*f);
    FPM->run(*f, *FAM);
    return f;
  }

  f->eraseFromParent();
  return nullptr;
}

Value *IfExprAST::codegen(){
  Value *condV = Cond->codegen();
  if(!condV)
    return nullptr;
  
  condV = builder->CreateFCmpONE(condV, ConstantFP::get(*context, APFloat(0.0)), "ifcond");

  Function *function = builder->GetInsertBlock()->getParent();
  BasicBlock *thenBB = BasicBlock::Create(*context, "then", function);
  BasicBlock *elseBB = BasicBlock::Create(*context, "else");
  BasicBlock *mergeBB = BasicBlock::Create(*context, "ifcont");

  builder->CreateCondBr(condV, thenBB, elseBB);

  builder->SetInsertPoint(thenBB);
  Value * thenV = Then->codegen();
  if(!thenV)
    return nullptr;
  
  builder->CreateBr(mergeBB);
  thenBB = builder->GetInsertBlock();

  function->insert(function->end(), elseBB);

  builder->SetInsertPoint(elseBB);
  Value *elseV = Else->codegen();
  if(!elseV)
    return nullptr;
  
  builder->CreateBr(mergeBB);
  elseBB = builder->GetInsertBlock();

  function->insert(function->end(), mergeBB);
  builder->SetInsertPoint(mergeBB);

  PHINode *PN = builder->CreatePHI(Type::getDoubleTy(*context), 2, "iftmp");
  PN->addIncoming(thenV, thenBB);
  PN->addIncoming(elseV, elseBB);
  
  return PN;
}

Value *ForExprAST::codegen(){
  Function *f = builder->GetInsertBlock()->getParent();
  AllocaInst *alloca = CreateEntryBlockAlloca(f, varName);

  Value* startVal = start->codegen();
  if(!startVal)
    return nullptr;

  builder->CreateStore(startVal, alloca);

  BasicBlock *PreheaderBB = builder->GetInsertBlock();
  BasicBlock *LoopBB = BasicBlock::Create(*context, "loop", f);

  builder->CreateBr(LoopBB);
  builder->SetInsertPoint(LoopBB);
  
  AllocaInst *oldVal = namedValues[varName];
  namedValues[varName] = alloca;

  if(!body->codegen())
    return nullptr;
  
  Value *stepVal = nullptr;

  if(step){
    stepVal = step->codegen();
    if(!stepVal)
      return nullptr;
  }else{
    stepVal = ConstantFP::get(*context, APFloat(1.0));
  }


  Value *endcond = end->codegen();

  if(!endcond)
    return nullptr;

  Value *curVar = builder->CreateLoad(alloca->getAllocatedType(), alloca, varName.c_str());
  Value *nextVar = builder->CreateAdd(curVar, stepVal, "nextVar");
  builder->CreateStore(nextVar, alloca);

  endcond = builder->CreateFCmpONE(endcond,
      ConstantFP::get(*context, APFloat(0.0)), "loopcond");
    
  BasicBlock *loopEndBB = builder->GetInsertBlock();
  BasicBlock *afterBB = BasicBlock::Create(*context, "afterloop", f);

  builder->CreateCondBr(endcond, LoopBB, afterBB);

  builder->SetInsertPoint(afterBB);

  if(oldVal)
    namedValues[varName] = oldVal;
  else  
    namedValues.erase(varName);

  return Constant::getNullValue(Type::getDoubleTy(*context));
}

Value *UnaryExprAST::codegen(){
  Value *operandV = operand->codegen();
  if(!operandV)
    return nullptr;
  
  Function *f = getFunction(std::string("unary") + opcode);
  if(!f)
    return LogErrorV("Unknown unary operator");
  
  return builder->CreateCall(f, operandV, "unop");
}