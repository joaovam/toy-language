#include"Lexer.h"

static int gettok(){
    static int LastChar = ' ';

    while(isspace(LastChar))
        LastChar = getchar();
    
    if(isalpha(LastChar)){
        IdentifierStr = LastChar;

        while(isalnum(LastChar = getchar()))
            IdentifierStr += LastChar;
        if(IdentifierStr == "def")
            return DEF;

        if(IdentifierStr == "extern")
            return EXTERN;

        return IDENTIFIER;
    }

    if(isdigit(LastChar) || LastChar == '.'){
        std::string numStr;

        do{
            numStr += LastChar;
            LastChar = getchar();
        }while(isdigit(LastChar) || LastChar == '.');

        NumVal = strtod(numStr.c_str(), 0);//add error checking
        return NUMBER;
    }

    if(LastChar == '#'){
        do
        LastChar = getchar();
        while(LastChar != EOF && LastChar != '\n' && LastChar != '\r');

        if(LastChar != EOF)
            return gettok();

        if(LastChar == EOF)
            return TK_EOF;
        
        int thisChar = LastChar;
        LastChar = getchar();
        return thisChar;
    }
}

static int CurTok;
static int getNextToken(){
    return CurTok = gettok();
}