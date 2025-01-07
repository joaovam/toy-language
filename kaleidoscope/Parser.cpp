#include "Parser.h"
#include "ErrorHandler.h"
#include "Lexer.h"
#include "KaleidoscopeJIT/KaleidoscopeJIT.h"

#include <iostream>

std::unique_ptr<LLVMContext> context;
std::unique_ptr<IRBuilder<>> builder;
std::unique_ptr<Module> module;
std::map<std::string, Value *> namedValues;
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

static std::unique_ptr<ExprAST> parsePrimary() {
  switch (CurTok) {
  case IDENTIFIER:
    return parseIdentifierExpr();
  case NUMBER:
    return parseNumberExpr();
  case '(':
    return parseParenExpr();

  default:
    return LogError("unknown token when expecting an expression");
  }
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

    auto RHS = parsePrimary();

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
  if (CurTok != IDENTIFIER)
    return LogErrorP("Expected function name in prototype");

  std::string fnName = IdentifierStr;
  getNextToken();

  if (CurTok != '(')
    LogErrorP("Expected '(' in prototype");

  std::vector<std::string> argNames;

  while (getNextToken() == IDENTIFIER)
    argNames.push_back(IdentifierStr);

  if (CurTok != ')')
    LogErrorP("Expected ')' in prototype");

  getNextToken();

  return std::make_unique<PrototypeAST>(fnName, std::move(argNames));
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
    
      auto ExprSymbol = ExitOnErr(JIT->lookup("__anon_expr"));;

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