#include <iostream>
#include"AST.h"
#include"Lexer.cpp"
#include"Parser.h"
#include "ErrorHandler.h"


static int getTokPrecedence(){
    if(!isascii(CurTok))
        return -1;
    
    int TokPrec = BinopPrecedence[CurTok];

    return (TokPrec <= 0) ? -1 : TokPrec;
}

static std::unique_ptr<ExprAST> parseNumberExpr(){
    auto result = std::make_unique<NumberExprAST>(NumVal);
    getNextToken();

    return std::move(result);
}

static std::unique_ptr<ExprAST> parsePrimary(){
    switch(CurTok){
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

static std::unique_ptr<ExprAST> parseExpression(){
    auto LHS = parsePrimary();
    if(!LHS)
        return nullptr;
    
    return parseBinOpRHS(0, std::move(LHS));
}

static std::unique_ptr<ExprAST> parseParenExpr(){
    getNextToken();
    auto v = parseExpression();

    if(!v){
        return nullptr;
    }

    if(CurTok != ')')
        return LogError("expected ')'");
    
    getNextToken();
    return v;
}

static std::unique_ptr<ExprAST> parseIdentifierExpr(){
    std::string idName = IdentifierStr;

    getNextToken();

    if(CurTok != '(')//means it is an identifier
        return std::make_unique<VariableExprAST>(idName);
    
    getNextToken();
    std::vector<std::unique_ptr<ExprAST>> args;

    if(CurTok != ')'){
        while(true){
        if(auto arg = parseExpression())
            args.push_back(std::move(arg));
        else
            return nullptr;
        
        if(CurTok == ')')
            break;

        if(CurTok != ',')
            return LogError("Expected ')' or ',' in argument list");
        getNextToken();
        }
    }
    getNextToken();

    return std::make_unique<CallExprAST>(idName, std::move(args));
}

//a + b * c| *( d + e)
static std::unique_ptr<ExprAST> parseBinOpRHS(int exprPrec, std::unique_ptr<ExprAST> LHS){
    while(true){
        int tkPrec = getTokPrecedence();

        if(tkPrec < exprPrec)
            return LHS;
        
        int binOp = CurTok;
        getNextToken();

        auto RHS = parsePrimary();

        if(!RHS)
            return nullptr;

        int nextPrec = getTokPrecedence();

        if(tkPrec < nextPrec){
            RHS = parseBinOpRHS(tkPrec + 1, std::move(RHS));
        }

        LHS = std::make_unique<BinaryExprAST>(binOp, std::move(LHS), std::move(RHS));
    }
}

static std::unique_ptr<PrototypeAST> parsePrototype(){
    if(CurTok != IDENTIFIER)
        return LogErrorP("Expected function name in prototype");

    std::string fnName = IdentifierStr;
    getNextToken();

    if(CurTok != '(')
        LogErrorP("Expected '(' in prototype");
    
    std::vector<std::string> argNames;

    while(getNextToken() == IDENTIFIER)
        argNames.push_back(IdentifierStr);
    
    if(CurTok != ')')
        LogErrorP("Expected ')' in prototype");

    
    getNextToken();

    return std::make_unique<PrototypeAST>(fnName, std::move(argNames));
}

static std::unique_ptr<FunctionAST> parseDefinition(){
    getNextToken();
    

    auto proto = parsePrototype();
    if(!proto)
        return nullptr;
    if(auto E = parseExpression())
        return std::make_unique<FunctionAST>(std::move(proto), std::move(E));
    
    return nullptr;
}

static std::unique_ptr<PrototypeAST> parseExtern(){
    getNextToken();
    return parsePrototype();
}

static std::unique_ptr<FunctionAST> parseTopLevelExpr(){
    if(auto E = parseExpression()){
        auto proto = std::make_unique<PrototypeAST>("__anon_expr", std::vector<std::string>());
        return std::make_unique<FunctionAST>(std::move(proto), std::move(E));
    }
    return nullptr;
}

static void handleDefinition() {
  if (parseDefinition()) {
    fprintf(stderr, "Parsed a function definition.\n");
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void handleExtern() {
  if (parseExtern()) {
    fprintf(stderr, "Parsed an extern\n");
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void handleTopLevelExpression() {
  // Evaluate a top-level expression into an anonymous function.
  if (parseTopLevelExpr()) {
    fprintf(stderr, "Parsed a top-level expr\n");
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

/// top ::= definition | external | expression | ';'
static void mainLoop() {
  while (true) {
    fprintf(stderr, "ready> ");
    
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
  }
}


int main(){
    BinopPrecedence['<'] = 10;
    BinopPrecedence['+'] = 20;
    BinopPrecedence['-'] = 20;
    BinopPrecedence['*'] = 40;  // highest.

    fprintf(stderr, "ready> ");
    getNextToken();

    mainLoop();
}