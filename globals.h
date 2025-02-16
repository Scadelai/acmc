#ifndef _GLOBALS_H_
#define _GLOBALS_H_

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

// Verifica se o compilador do Yacc/Bison está definindo YYPARSER; se não, inclui o cabeçalho gerado
#ifndef YYPARSER
#include "acmc.tab.h"  // Cabeçalho gerado pelo Yacc/Bison

// Define ENDFILE para indicar o fim do arquivo (não incluído automaticamente pelo Yacc/Bison)
#define ENDFILE 0
#endif

// Define os valores booleanos FALSE e TRUE, caso não estejam definidos
#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

// Número máximo de palavras reservadas na linguagem
#define MAXRESERVED 8

// Tipo de token utilizado pelo Yacc/Bison
typedef int TokenType;

// Ponteiros para os arquivos de código fonte e de listagem de saída
extern FILE *source;   // Arquivo de código fonte
extern FILE *listing;  // Arquivo de saída para listagem

// Número da linha atual do código fonte para relatórios
extern int lineno;

/**************************************************/
/********** Árvore Sintática para Parsing ********/
/**************************************************/

// Tipos de nós na árvore sintática: declaração (StmtK) ou expressão (ExpK)
typedef enum {StmtK, ExpK} NodeKind;

// Tipos de declarações
typedef enum {IfK, WhileK, AssignK, ReturnK} StmtKind;

// Tipos de expressões
typedef enum {OpK, ConstK, IdK, VarK, TypeK, ParamK, FuncK, CallK} ExpKind;

// Tipos de dados para verificação de tipos em expressões
typedef enum {Void, Integer, Boolean} ExpType;
typedef enum {fun, var} IDType;
typedef enum {intDType, voidDType} DataType;

// Número máximo de filhos para cada nó na árvore sintática
#define MAXCHILDREN 3

// Estrutura do nó da árvore sintática
typedef struct treeNode {
  struct treeNode *child[MAXCHILDREN];  // Ponteiros para os nós filhos
  struct treeNode *sibling;             // Ponteiro para o nó irmão
  int lineno;                           // Número da linha do código fonte
  int add;                              // Indicador para marcação (uso interno)
  int size;                             // Tamanho (uso específico)
  NodeKind nodekind;                    // Tipo do nó (StmtK ou ExpK)
  union {
    StmtKind stmt;                      // Tipo de declaração, se aplicável
    ExpKind exp;                        // Tipo de expressão, se aplicável
  } kind;
  union {
    TokenType opr;                      // Operador (para expressões)
    int val;                            // Valor constante
    char *name;                         // Nome do identificador
  } attr;
  DataType type;                        // Tipo de dado para verificação de tipos
} TreeNode;

// Flag global que indica se ocorreu algum erro; se TRUE, interrompe as próximas análises
extern int Error;

#endif
