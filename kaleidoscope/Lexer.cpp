#include "Lexer.h"

#include <iostream>

// Global variables
std::string IdentifierStr;
double NumVal;
int CurTok;

static int LastChar = ' ';

int gettok() {

  while (isspace(LastChar))
    LastChar = getchar();

  if (isalpha(LastChar)) {
    IdentifierStr = LastChar;

    while (isalnum(LastChar = getchar()))
      IdentifierStr += LastChar;
    if (IdentifierStr == "def")
      return DEF;

    if (IdentifierStr == "extern")
      return EXTERN;
    
    if(IdentifierStr == "if")
      return IF;
    
    if (IdentifierStr == "then")
      return THEN;
    
    if (IdentifierStr == "else")
      return ELSE;
    if (IdentifierStr == "for")
      return FOR;
    if (IdentifierStr == "in")
      return IN;
    if(IdentifierStr == "unary")
      return UNARY;
    if(IdentifierStr == "binary")
      return BINARY;
    if(IdentifierStr == "var")
      return VAR;
      
    return IDENTIFIER;
  }

  if (isdigit(LastChar) || LastChar == '.') {
    std::string numStr;

    do {
      numStr += LastChar;
      LastChar = getchar();
    } while (isdigit(LastChar) || LastChar == '.');

    NumVal = strtod(numStr.c_str(), 0); // add error checking
    return NUMBER;
  }

  if (LastChar == '#') {
    do
      LastChar = getchar();
    while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

    if (LastChar != EOF)
      return gettok();
  }
  if (LastChar == EOF)
    return TK_EOF;

  int thisChar = LastChar;
  LastChar = getchar();
  return thisChar;
}

int getNextToken() {
  CurTok = gettok();
  return CurTok;
}