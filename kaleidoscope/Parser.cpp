#include "Parser.h"
#include "ErrorHandler.h"
#include "Lexer.h"
#include "KaleidoscopeJIT/KaleidoscopeJIT.h"

#include <iostream>

std::unique_ptr<LLVMContext> context;
std::unique_ptr<IRBuilder<>> builder;
std::unique_ptr<Module> module;
std::map<std::string, AllocaInst*> namedValues;
std::unique_ptr<KaleidoscopeJIT> JIT;
std::unique_ptr<FunctionPassManager> FPM;
std::unique_ptr<LoopAnalysisManager> LAM;
std::unique_ptr<FunctionAnalysisManager> FAM;
std::unique_ptr<CGSCCAnalysisManager> CGAM;
std::unique_ptr<ModuleAnalysisManager> MAM;
std::unique_ptr<PassInstrumentationCallbacks> PIC;
std::unique_ptr<StandardInstrumentations> SI;
std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
ExitOnError ExitOnErr;
std::map<char, int> BinopPrecedence;



static int getTokPrecedence() {
  if (!isascii(CurTok))
    return -1;

  int TokPrec = BinopPrecedence[CurTok];

  return (TokPrec <= 0) ? -1 : TokPrec;
}

static std::unique_ptr<ExprAST> parseNumberExpr() {
  auto result = std::make_unique<NumberExprAST>(NumVal);
  getNextToken();

  return std::move(result);
}

static std::unique_ptr<ExprAST> parseIfExpr(){
  getNextToken();
  auto cond = parseExpression();
  if(!cond)
    return nullptr;
  
  if(CurTok != THEN)
    return LogError("expected then");
  
  getNextToken();
  auto then = parseExpression();
  if(!then)
    return nullptr;
  
  if(CurTok != ELSE)
    return LogError("expected else");
  
  getNextToken();

  auto Else = parseExpression();
  if(!Else)
    return nullptr;
  
  return std::make_unique<IfExprAST>(std::move(cond), std::move(then), std::move(Else));
}

static std::unique_ptr<ExprAST> parseForExpr(){
  getNextToken();
  if(CurTok != IDENTIFIER)
    return LogError("expected identifier after for");
  
  std::string idName = IdentifierStr;
  getNextToken();

  if(CurTok != '=')
    return LogError("expected '=' after for");
  
  getNextToken();
  auto start = parseExpression();
  if(!start)
    return nullptr;
  if(CurTok != ',')
    return LogError("expected ',' after for start value");
  getNextToken();


  auto end = parseExpression();
  if(!end)
    return nullptr;

  std::unique_ptr<ExprAST> step;

  if(CurTok == ','){
    getNextToken();
    step = parseExpression();
    if(!step)
      return nullptr;
  }

  if(CurTok != IN)
    return LogError("expected in after for");
  
  getNextToken();

  auto body = parseExpression();
  if(!body)
    return nullptr;

    return std::make_unique<ForExprAST>(idName, std::move(start), std::move(end), 
    std::move(step), std::move(body));
}

static std::unique_ptr<ExprAST> parsePrimary() {
  switch (CurTok) {
  case IDENTIFIER:
    return parseIdentifierExpr();
  case NUMBER:
    return parseNumberExpr();
  case '(':
    return parseParenExpr();
  case IF:
    return parseIfExpr();
  case FOR:
    return parseForExpr();
  default:
    return LogError("unknown token when expecting an expression");
  }
}

static std::unique_ptr<ExprAST> parseUnary(){
  if(!isascii(CurTok) || CurTok == '(' || CurTok == ',')
    return parsePrimary();
  
  int opc = CurTok;
  getNextToken();
  if(auto operand = parseUnary())
    return std::make_unique<UnaryExprAST>(opc, std::move(operand));
  
  return nullptr;
}

static std::unique_ptr<ExprAST> parseExpression() {
  auto LHS = parsePrimary();
  if (!LHS)
    return nullptr;

  return parseBinOpRHS(0, std::move(LHS));
}

static std::unique_ptr<ExprAST> parseParenExpr() {
  getNextToken();
  auto v = parseExpression();

  if (!v) {
    return nullptr;
  }

  if (CurTok != ')')
    return LogError("expected ')'");

  getNextToken();
  return v;
}

static std::unique_ptr<ExprAST> parseIdentifierExpr() {
  std::string idName = IdentifierStr;

  getNextToken();

  if (CurTok != '(') // means it is an identifier
    return std::make_unique<VariableExprAST>(idName);

  getNextToken();
  std::vector<std::unique_ptr<ExprAST>> args;

  if (CurTok != ')') {
    while (true) {
      if (auto arg = parseExpression())
        args.push_back(std::move(arg));
      else
        return nullptr;

      if (CurTok == ')')
        break;

      if (CurTok != ',')
        return LogError("Expected ')' or ',' in argument list");
      getNextToken();
    }
  }
  getNextToken();

  return std::make_unique<CallExprAST>(idName, std::move(args));
}

// a + b * c| *( d + e)
static std::unique_ptr<ExprAST> parseBinOpRHS(int exprPrec,
                                              std::unique_ptr<ExprAST> LHS) {
  while (true) {
    int tkPrec = getTokPrecedence();

    if (tkPrec < exprPrec)
      return LHS;

    int binOp = CurTok;
    getNextToken();

    auto RHS = parseUnary();

    if (!RHS)
      return nullptr;

    int nextPrec = getTokPrecedence();

    if (tkPrec < nextPrec) {
      RHS = parseBinOpRHS(tkPrec + 1, std::move(RHS));
    }

    LHS =
        std::make_unique<BinaryExprAST>(binOp, std::move(LHS), std::move(RHS));
  }
}

static std::unique_ptr<PrototypeAST> parsePrototype() {
  unsigned kind = 0;
  unsigned binaryPrecedence = 30;
  std::string fnName;

  switch (CurTok){
    default:
      return LogErrorP("Expected function name in prototype");
    case IDENTIFIER:
    fnName = IdentifierStr;
    kind = 0;
    getNextToken();
    break;

    case UNARY:
      getNextToken();
      if(!isascii((CurTok)))
        return LogErrorP("Expected unary operator");
      fnName = "unary";
      fnName += (char) CurTok;
      kind = 1;
      getNextToken();
      break;

    case BINARY:
      getNextToken();
      if(!isascii(CurTok))
        return LogErrorP("Expected binary operator");
      fnName = "binary";
      fnName +=(char)CurTok;
      kind = 2;
      getNextToken();
      if(CurTok == NUMBER){
        if(NumVal <1 || NumVal > 100)
          return LogErrorP("Invalid precedence: must be 1..100");
          binaryPrecedence = (unsigned) NumVal;
          getNextToken();
      }
      break;
  }

  if(CurTok != '(')
    return LogErrorP("Expected '(' in prototype");
  
  std::vector<std::string> argNames;

  while(getNextToken() == IDENTIFIER)
    argNames.push_back(IdentifierStr);
  
  if(CurTok != ')')
    return LogErrorP("Expected ')' in prototype");
  
  getNextToken();
  if(kind && argNames.size() != kind)
    return LogErrorP("Invalid number of operands for operator");

  return std::make_unique<PrototypeAST>(fnName, std::move(argNames), kind!= 0, binaryPrecedence);
}

static std::unique_ptr<FunctionAST> parseDefinition() {
  getNextToken();

  auto proto = parsePrototype();
  if (!proto)
    return nullptr;
  if (auto E = parseExpression())
    return std::make_unique<FunctionAST>(std::move(proto), std::move(E));

  return nullptr;
}

static std::unique_ptr<PrototypeAST> parseExtern() {
  getNextToken();
  return parsePrototype();
}

static std::unique_ptr<FunctionAST> parseTopLevelExpr() {
  if (auto E = parseExpression()) {
    auto proto = std::make_unique<PrototypeAST>("__anon_expr",
                                                std::vector<std::string>());
    return std::make_unique<FunctionAST>(std::move(proto), std::move(E));
  }
  return nullptr;
}

static void InitializeModule() {
  context = std::make_unique<LLVMContext>();
  module = std::make_unique<Module>("KaleidoscopeJIT", *context);
  module->setDataLayout(JIT->getDataLayout());

  // create a new builder for the module
  builder = std::make_unique<IRBuilder<>>(*context);

  FPM = std::make_unique<FunctionPassManager>();

  LAM = std::make_unique<LoopAnalysisManager>();

  FAM = std::make_unique<FunctionAnalysisManager>();

  CGAM = std::make_unique<CGSCCAnalysisManager>();

  MAM = std::make_unique<ModuleAnalysisManager>();

  PIC = std::make_unique<PassInstrumentationCallbacks>();

  SI = std::make_unique<StandardInstrumentations>(*context, true);

  SI->registerCallbacks(*PIC, MAM.get());
  FPM->addPass(PromotePass());
  FPM->addPass(InstCombinePass());
  FPM->addPass(ReassociatePass());
  FPM->addPass(GVNPass());
  FPM->addPass(SimplifyCFGPass());

  PassBuilder PB;
  PB.registerModuleAnalyses(*MAM);
  PB.registerFunctionAnalyses(*FAM);
  PB.crossRegisterProxies(*LAM, *FAM, *CGAM, *MAM);
}

static void handleDefinition() {
  if (auto fnAST = parseDefinition()) {
    if (auto *fnIR = fnAST->codegen()) {
      auto TSM = ThreadSafeModule(std::move(module), std::move(context));
      ExitOnErr(JIT->addModule(std::move(TSM)));
      InitializeModule();
    }
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void handleExtern() {
  if (auto protoAST = parseExtern()) {
    if (auto *fnIR = protoAST->codegen()) {
      fnIR->print(errs());
      FunctionProtos[protoAST->getName()] = std::move(protoAST);
    }
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void handleTopLevelExpression() {

  // Evaluate a top-level expression into an anonymous function.
  if (auto fnAST = parseTopLevelExpr()) {
    
    if (fnAST->codegen()) {
      auto RT = JIT->getMainJITDylib().createResourceTracker();

      auto TSM = ThreadSafeModule(std::move(module), std::move(context));

      ExitOnErr(JIT->addModule(std::move(TSM), RT));
      InitializeModule();
    
      auto ExprSymbol = ExitOnErr(JIT->lookup("__anon_expr"));

      double (*FP)() = ExprSymbol.getAddress().toPtr<double(*)()>();
      fprintf(stderr, "Evaluated to %f\n", FP());

      ExitOnErr(RT->remove());
    }
  } else {
    getNextToken();
  }
}

/// top ::= definition | external | expression | ';'
static void mainLoop() {
  while (true) {
    switch (CurTok) {
    case TK_EOF:
      return;
    case ';': // ignore top-level semicolons.
      getNextToken();

      break;
    case DEF:

      handleDefinition();
      break;
    case EXTERN:

      handleExtern();
      break;
    default:

      handleTopLevelExpression();
      break;
    }
    fprintf(stderr, "ready> ");
  }
}

//===----------------------------------------------------------------------===//
// "Library" functions that can be "extern'd" from user code.
//===----------------------------------------------------------------------===//

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

/// putchard - putchar that takes a double and returns 0.
extern "C" DLLEXPORT double putchard(double X) {
  fputc((char)X, stderr);
  return 0;
}

/// printd - printf that takes a double prints it as "%f\n", returning 0.
extern "C" DLLEXPORT double printd(double X) {
  fprintf(stderr, "%f\n", X);
  return 0;
}



int main() {
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40; // highest.

  fprintf(stderr, "ready> ");
  getNextToken();

  JIT = ExitOnErr(KaleidoscopeJIT::Create());

  InitializeModule();
  mainLoop();
  module->print(errs(), nullptr);
}