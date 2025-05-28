#ifndef _CODEGEN_H_
#define _CODEGEN_H_

#include "globals.h"

// Maximum length for temporary variable names and labels
#define MAX_TEMP_LEN 20
#define MAX_LABEL_LEN 20

// Structure to hold information about global variables
typedef struct globalVarRec {
    char *name;
    int size;  // 1 for simple variables, >1 for arrays
    struct globalVarRec *next;
} *GlobalVarList;

// Code generation functions
void codeGen(TreeNode *syntaxTree, char *irOutputFile);
void generateIntermediateCode(TreeNode *syntaxTree);

// Utility functions for code generation
char *newTemp(void);
char *newLabel(void);
char *generate_expression_code(TreeNode *tree);

#endif