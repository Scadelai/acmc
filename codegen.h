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
void codeGen(TreeNode *syntaxTree);
void emitComment(char *comment);
void emitGlobalVar(char *name, int size);
void emitFuncHeader(char *funcName, TreeNode *params);
void emitFuncEnd(void);
void emitAssign(char *target, char *source);
void emitArrayAssign(char *arrayName, char *index, char *value);
void emitArrayLoad(char *target, char *arrayName, char *index);
void emitOp(char *target, char *arg1, char *op, char *arg2);
void emitCall(char *target, char *funcName, TreeNode *args);
void emitCallVoid(char *funcName, TreeNode *args);
void emitLabel(char *label);
void emitGoto(char *label);
void emitCondGoto(char *condition, char *label);
void emitLabelCondGoto(char *labelName, char *condition, char *gotoLabel);
void emitLabelReturn(char *labelName, char *value);
void emitLabelInstruction(char *labelName, const char *instruction);
void emitReturn(char *value);
void emitReturnVoid(void);

// Utility functions for code generation
char *newTemp(void);
char *newLabel(void);
char *generateExpression(TreeNode *tree);
void generateStatement(TreeNode *tree);
void generateFunction(TreeNode *tree);
void generateGlobalDeclarations(TreeNode *tree);

#endif