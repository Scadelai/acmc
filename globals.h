/****************************************************/
/* File: globals.h                                  */
/* Yacc/Bison Version                               */
/* Global types and vars for TINY compiler          */
/* must come before other include files             */
/* Compiler Construction: Principles and Practice   */
/* Kenneth C. Louden                                */
/****************************************************/

#ifndef _GLOBALS_H_
#define _GLOBALS_H_

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

/* Yacc/Bison generates internally its own values
 * for the tokens. Other files can access these values
 * by including the tab.h file generated using the
 * Yacc/Bison option -d ("generate header")
 *
 * The YYPARSER flag prevents inclusion of the tab.h
 * into the Yacc/Bison output itself
 */

#ifndef YYPARSER

/* the name of the following file may change */
#include "acmc.tab.h"

/* ENDFILE is implicitly defined by Yacc/Bison,
 * and not included in the tab.h file
 */
#define ENDFILE 0

#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

/* MAXRESERVED = the number of reserved words */
#define MAXRESERVED 8

/* Yacc/Bison generates its own integer values
 * for tokens
 */
typedef int TokenType;

extern FILE *source; /* source code text file */
extern FILE *listing; /* listing output text file */

extern int lineno; /* source line number for listing */

/**************************************************/
/***********   Syntax tree for parsing ************/
/**************************************************/

typedef enum {StmtK, ExpK} NodeKind;
typedef enum {IfK, WhileK, AssignK, ReturnK} StmtKind;
typedef enum {OpK, ConstK, IdK, VarK, TypeK, ParamK, FuncK, CallK} ExpKind;

/* ExpType is used for type checking */
typedef enum {Void,Integer,Boolean} ExpType;
typedef enum {fun, var} IDType;
typedef enum {intDType, voidDType} DataType;

#define MAXCHILDREN 3

typedef struct treeNode {
  struct treeNode *child[MAXCHILDREN];
  struct treeNode *sibling;
  int lineno;
  int add;
  int size;
  NodeKind nodekind;
  union {
    StmtKind stmt;
    ExpKind exp;
  } kind;
  union {
    TokenType opr;
    int val;
    char *name;
  } attr;
  DataType type; /* for type checking of exps */
} TreeNode;

/* Error = TRUE prevents further passes if an error occurs */
extern int Error;


#endif
