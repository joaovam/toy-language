#include<map>
static std::map<char, int> BinopPrecedence;
static std::unique_ptr<ExprAST> parseBinOpRHS(int exprPrec, std::unique_ptr<ExprAST> LHS);
static std::unique_ptr<ExprAST> parseIdentifierExpr();
static std::unique_ptr<ExprAST> parseExpression();
static std::unique_ptr<FunctionAST> parseDefinition();
static std::unique_ptr<ExprAST> parseIdentifierExpr();
static std::unique_ptr<ExprAST> parseParenExpr();