#include<string>

enum Token{
    TK_EOF = -1,

    //commands
    DEF = -2,
    EXTERN = -3,

    //primary
    IDENTIFIER = -4,
    NUMBER = -5
};

static std::string IdentifierStr;
static double NumVal;