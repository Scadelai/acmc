%{
#include "acmc.tab.h"   /* Cabeçalho gerado pelo Bison contendo as definições dos tokens */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int line_num = 1;

void lex_error(const char *s);
/* Redefine yyerror para lex_error neste arquivo */
#define yyerror lex_error
%}

%option noyywrap

%%
"/*"    { 
            char c;
            int ast = 0;
            do {
                c = input();
                if (c == EOF) break;
                if (c == '\n') line_num++;
                if (c == '*') {
                    ast = 1;
                } else {
                    ast = 0;
                }
            } while (ast != 1 || c != '/');
        }

"if"            { return IF; }
"else"          { return ELSE; }
"while"         { return WHILE; }
"return"        { return RETURN; }
"int"           { return INT; }
"void"          { return VOID; }

[ \t\r]+        { /* ignora espaços em branco */ }
\n              { line_num++; }

"=="            { return EQ; }
"!="            { return NE; }
"<="            { return LE; }
">="            { return GE; }
"<"             { return LT; }
">"             { return GT; }

"="             { return ASSIGN; }
";"             { return SEMI; }
","             { return COMMA; }
"("             { return LPAREN; }
")"             { return RPAREN; }
"["             { return LBRACKET; }
"]"             { return RBRACKET; }
"{"             { return LBRACE; }
"}"             { return RBRACE; }
"+"             { return PLUS; }
"-"             { return MINUS; }
"*"             { return TIMES; }
"/"             { return DIVIDE; }

[0-9]+          { 
                    yylval.num = atoi(yytext); 
                    return NUM; 
                 }

[a-zA-Z][a-zA-Z0-9]* { 
                    yylval.id = strdup(yytext);
                    return ID;
                  }

.               { 
                    printf("ERRO LÉXICO: %s LINHA: %d\n", yytext, line_num); 
                  }

%%

/* Definição da função de erro léxico */
void lex_error(const char *s) {
    fprintf(stderr, "ERRO LÉXICO: %s LINHA: %d\n", s, line_num);
}
