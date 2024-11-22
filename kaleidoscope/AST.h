#include<string>
#include<memory>
#include<vector>

class ExprAST{
    public:
    virtual ~ExprAST() = default;
};

class NumberExprAST : public ExprAST{
    double val;
    public:
    NumberExprAST(double val) : val(val){}
};

class VariableExprAST : public ExprAST{
    std::string name;

    public:
    VariableExprAST(const std::string &name) : name(name){}
};

class BinaryExprAST : public ExprAST{
    char op;
    std::unique_ptr<ExprAST> LHS, RHS;

    public:
    BinaryExprAST(char op, std::unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS) 
    : op(op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
};

class CallExprAST : public ExprAST{
    std::string callee;
    std::vector<std::unique_ptr<ExprAST>> args;

    public:
    CallExprAST(const std::string & callee,
    std::vector<std::unique_ptr<ExprAST>> args)
    : callee(callee), args(std::move(args)) {}
};

class PrototypeAST{
    std::string name;
    std::vector<std::string> args;

    public:
    PrototypeAST(const std::string &name, std::vector<std::string> args)
    : name(name), args(std::move(args)){}
    const std::string &getName() const {return name;}
};

class FunctionAST{
    std::unique_ptr<PrototypeAST> proto;
    std::unique_ptr<ExprAST> body;

    public:
    FunctionAST(std::unique_ptr<PrototypeAST> proto,
    std::unique_ptr<ExprAST> body)
    : proto(std::move(proto)), body(std::move(body)) {}
};