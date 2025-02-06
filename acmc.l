%{
#include "globals.h"
#include "util.h"
#include "scan.h"

static int yylex(void);

/* lexeme of identifier or reserved word */
char tokenString[MAXTOKENLEN+1];
%}

%option noyywrap

digito       [0-9]
numero      {digito}+
letra      [a-zA-Z]
identificador  {letra}+
newline     \n
whitespace  [ \t\r]+

%%

"void"			 {return VOID;}
"if"         {return IF;}
"int"        {return INT;}
"return"     {return RETURN;}
"while"      {return WHILE;}
"else"       {return ELSE;}

"+"          {return MAIS;}
"-"          {return SUB;}
"*"          {return MULT;}
"/"          {return DIV;}
"="          {return IGUAL;}

"<"          {return MENOR;}
"<="	       {return MENIG;}
">"	         {return MAIOR;}
">="	       {return MAIIG;}
"=="         {return IGDAD;}
"!="         { return DIFER;}

";"          {return PV;}
","          {return VIR;}
"("          {return APAR;}
")"          {return FPAR;}
"["          {return ACOL;}
"]"          {return FCOL;}
"{"          {return ACHAV;}
"}"          {return FCHAV;}

{numero}        {return NUM;}
{identificador} {return ID;}
{newline}       {lineno++;}
{whitespace}    {/* skip whitespace */}
"/*"             { char c;
                  char p='\0';
                  do
                  { c = input();
                    if (c == EOF) break;
                    if (c == '\n') lineno++;
                    if (p == '*' && c == '/') break;
                    p = c;
                  } while (1);
                }
.               {return ERROR;}

%%

TokenType getToken(void)
{ static int firstTime = TRUE;
  TokenType currentToken;
  if (firstTime)
  { firstTime = FALSE;
    lineno++;
    yyin = source;
    yyout = listing;
  }
  currentToken = yylex();
  strncpy(tokenString,yytext,MAXTOKENLEN);
  return currentToken;
}
