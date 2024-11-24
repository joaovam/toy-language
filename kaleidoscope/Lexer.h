#ifndef LEXER_H
#define LEXER_H

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

extern std::string IdentifierStr;
extern double NumVal;
extern int CurTok;

int gettok();
int getNextToken();
#endif