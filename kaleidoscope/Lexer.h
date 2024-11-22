#include<string>

enum Token{
    TK_EOF = -1,

    //commands
    DEF = -2,
    EXTERN = -3,

    //primary
    IDENTIFIER,
    NUMBER
};

static std::string IdentifierStr;
static double NumVal;