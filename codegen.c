#include "globals.h"
#include "codegen.h"
#include "util.h"
#include <string.h>
#include <stdarg.h>

// Global variables for code generation
static FILE *outputFile;
static int tempCount = 1;    // Counter for temporary variables
static int labelCount = 1;   // Counter for labels
static GlobalVarList globalVars = NULL;  // List of global variables

// Helper function to get operator string
static char *getOpString(TokenType op) {
    switch (op) {
        case MAIS: return "+";
        case SUB: return "-";
        case MULT: return "*";
        case DIV: return "/";
        case MENOR: return "<";
        case MENIG: return "<=";
        case MAIOR: return ">";
        case MAIIG: return ">=";
        case IGDAD: return "==";
        case DIFER: return "!=";
        default: return "?";
    }
}

// Helper function to add global variable to list
static void addGlobalVar(char *name, int size) {
    GlobalVarList newVar = (GlobalVarList)malloc(sizeof(struct globalVarRec));
    newVar->name = copyString(name);
    newVar->size = size;
    newVar->next = globalVars;
    globalVars = newVar;
}

// Check if a variable is global
static int isGlobalVar(char *name) {
    GlobalVarList curr = globalVars;
    while (curr != NULL) {
        if (strcmp(curr->name, name) == 0) {
            return 1;
        }
        curr = curr->next;
    }
    return 0;
}

// Emit functions
void emitComment(char *comment) {
    fprintf(outputFile, "// %s\n", comment);
}

void emitGlobalVar(char *name, int size) {
    if (size > 1) {
        fprintf(outputFile, "GLOBAL %s[%d]\n", name, size);
    } else {
        fprintf(outputFile, "GLOBAL %s\n", name);
    }
}

void emitFuncHeader(char *funcName, TreeNode *params) {
    fprintf(outputFile, "FUNC %s", funcName);
    if (params != NULL) {
        TreeNode *param = params;
        while (param != NULL) {
            // Debug: check what kind of node this is
            if (param->nodekind == ExpK) {
                if (param->kind.exp == ParamK) {
                    fprintf(outputFile, " %s", param->attr.name);
                } else if (param->kind.exp == TypeK && param->child[0] != NULL && param->child[0]->kind.exp == ParamK) {
                    // Parameter is nested under TypeK node
                    fprintf(outputFile, " %s", param->child[0]->attr.name);
                }
                if (param->sibling != NULL && 
                    ((param->sibling->kind.exp == ParamK) || 
                     (param->sibling->kind.exp == TypeK && param->sibling->child[0] != NULL && param->sibling->child[0]->kind.exp == ParamK))) {
                    fprintf(outputFile, ",");
                }
            }
            param = param->sibling;
        }
    }
    fprintf(outputFile, "\n");
}

void emitFuncEnd(void) {
    fprintf(outputFile, "END FUNC\n");
}

void emitAssign(char *target, char *source) {
    fprintf(outputFile, "    %s = %s\n", target, source);
}

void emitArrayAssign(char *arrayName, char *index, char *value) {
    fprintf(outputFile, "    %s[%s] = %s\n", arrayName, index, value);
}

void emitArrayLoad(char *target, char *arrayName, char *index) {
    fprintf(outputFile, "    %s = %s[%s]\n", target, arrayName, index);
}

void emitOp(char *target, char *arg1, char *op, char *arg2) {
    fprintf(outputFile, "    %s = %s %s %s\n", target, arg1, op, arg2);
}

void emitCall(char *target, char *funcName, TreeNode *args) {
    char argBuffer[1000] = "";
    if (args != NULL) {
        TreeNode *arg = args;
        int first = 1;
        while (arg != NULL) {
            char *argStr = generateExpression(arg);
            if (!first) {
                strcat(argBuffer, ", ");
            }
            strcat(argBuffer, argStr);
            first = 0;
            arg = arg->sibling;
        }
    }
    fprintf(outputFile, "    %s = call %s(%s)\n", target, funcName, argBuffer);
}

void emitCallVoid(char *funcName, TreeNode *args) {
    char argBuffer[1000] = "";
    if (args != NULL) {
        TreeNode *arg = args;
        int first = 1;
        while (arg != NULL) {
            char *argStr = generateExpression(arg);
            if (!first) {
                strcat(argBuffer, ", ");
            }
            strcat(argBuffer, argStr);
            first = 0;
            arg = arg->sibling;
        }
    }
    fprintf(outputFile, "    call %s(%s)\n", funcName, argBuffer);
}

void emitLabel(char *label) {
    fprintf(outputFile, "%s: ", label);
}

void emitLabelNewline(char *label) {
    fprintf(outputFile, "%s:\n", label);
}

void emitGoto(char *label) {
    fprintf(outputFile, "    goto %s\n", label);
}

void emitCondGoto(char *condition, char *label) {
    fprintf(outputFile, "    if %s goto %s\n", condition, label);
}

void emitLabelCondGoto(char *labelName, char *condition, char *gotoLabel) {
    fprintf(outputFile, "%s: if %s goto %s\n", labelName, condition, gotoLabel);
}

void emitLabelReturn(char *labelName, char *value) {
    if (value != NULL) {
        fprintf(outputFile, "%s: return %s\n", labelName, value);
    } else {
        fprintf(outputFile, "%s: return\n", labelName);
    }
}

void emitLabelInstruction(char *labelName, const char *instruction) {
    fprintf(outputFile, "%s: %s\n", labelName, instruction);
}

void emitReturn(char *value) {
    fprintf(outputFile, "    return %s\n", value);
}

void emitReturnVoid(void) {
    fprintf(outputFile, "    return\n");
}

void emitReturnAfterLabel(char *value) {
    if (value != NULL) {
        fprintf(outputFile, "return %s\n", value);
    } else {
        fprintf(outputFile, "return\n");
    }
}

// Utility functions
char *newTemp(void) {
    char *temp = (char *)malloc(MAX_TEMP_LEN);
    snprintf(temp, MAX_TEMP_LEN, "t%d", tempCount++);
    return temp;
}

char *newLabel(void) {
    char *label = (char *)malloc(MAX_LABEL_LEN);
    snprintf(label, MAX_LABEL_LEN, "L%d", labelCount++);
    return label;
}

// Generate code for expressions
char *generateExpression(TreeNode *tree) {
    char *result = NULL;
    char *left, *right, *temp;
    
    if (tree == NULL) return NULL;
    
    switch (tree->nodekind) {
        case ExpK:
            switch (tree->kind.exp) {
                case ConstK:
                    result = (char *)malloc(20);
                    snprintf(result, 20, "%d", tree->attr.val);
                    break;
                    
                case IdK:
                    if (tree->child[0] != NULL) {
                        // Array access: var[index]
                        char *index = generateExpression(tree->child[0]);
                        temp = newTemp();
                        emitArrayLoad(temp, tree->attr.name, index);
                        result = temp;
                    } else {
                        // Simple variable
                        result = copyString(tree->attr.name);
                    }
                    break;
                    
                case VarK:
                    if (tree->child[0] != NULL) {
                        // Array access: var[index]
                        char *index = generateExpression(tree->child[0]);
                        temp = newTemp();
                        emitArrayLoad(temp, tree->attr.name, index);
                        result = temp;
                    } else {
                        // Simple variable
                        result = copyString(tree->attr.name);
                    }
                    break;
                    
                case OpK:
                    left = generateExpression(tree->child[0]);
                    right = generateExpression(tree->child[1]);
                    
                    // For all operators, use temporary variable
                    temp = newTemp();
                    emitOp(temp, left, getOpString(tree->attr.opr), right);
                    result = temp;
                    break;
                    
                case CallK:
                    // Process arguments first to ensure proper temporary ordering
                    char argBuffer[1000] = "";
                    if (tree->child[0] != NULL) {
                        TreeNode *arg = tree->child[0];
                        int first = 1;
                        while (arg != NULL) {
                            char *argStr = generateExpression(arg);
                            if (!first) {
                                strcat(argBuffer, ", ");
                            }
                            strcat(argBuffer, argStr);
                            first = 0;
                            arg = arg->sibling;
                        }
                    }
                    // Get temporary after processing arguments
                    temp = newTemp();
                    fprintf(outputFile, "    %s = call %s(%s)\n", temp, tree->attr.name, argBuffer);
                    result = temp;
                    break;
                    
                default:
                    result = copyString("?");
                    break;
            }
            break;
            
        default:
            result = copyString("?");
            break;
    }
    
    return result;
}

// Generate code for statements
void generateStatement(TreeNode *tree) {
    char *condition, *label1, *label2, *value;
    
    if (tree == NULL) return;
    
    switch (tree->nodekind) {
        case StmtK:
            switch (tree->kind.stmt) {
                case AssignK:
                    // Check if left side is array access (VarK or IdK with child)
                    if ((tree->child[0]->kind.exp == VarK && tree->child[0]->child[0] != NULL) ||
                        (tree->child[0]->kind.exp == IdK && tree->child[0]->child[0] != NULL)) {
                        // Array assignment: arr[index] = value
                        char *index = generateExpression(tree->child[0]->child[0]);
                        char *val = generateExpression(tree->child[1]);
                        emitArrayAssign(tree->child[0]->attr.name, index, val);
                    } else {
                        // Simple assignment: var = value
                        if (tree->child[1]->kind.exp == CallK) {
                            // Direct function call assignment: generate temp = call, then var = temp
                            char *temp = newTemp();
                            emitCall(temp, tree->child[1]->attr.name, tree->child[1]->child[0]);
                            emitAssign(tree->child[0]->attr.name, temp);
                        } else {
                            char *val = generateExpression(tree->child[1]);
                            emitAssign(tree->child[0]->attr.name, val);
                        }
                    }
                    break;
                    
                case IfK:
                    // For if statements, we need to handle the condition specially
                    if (tree->child[0]->kind.exp == OpK) {
                        // Generate condition operands as temporaries if they are complex expressions
                        char *left = generateExpression(tree->child[0]->child[0]);
                        char *right = generateExpression(tree->child[0]->child[1]);
                        char *op = getOpString(tree->child[0]->attr.opr);
                        condition = (char *)malloc(100);
                        snprintf(condition, 100, "%s %s %s", left, op, right);
                    } else {
                        condition = generateExpression(tree->child[0]);
                    }
                    
                    label1 = newLabel();
                    if (tree->child[2] != NULL) {
                        // if-else: if condition goto then_label, else_code, then_label: then_code
                        emitCondGoto(condition, label1);
                        generateStatement(tree->child[2]); // else part
                        // Check if the then part is a simple return statement
                        if (tree->child[1]->nodekind == StmtK && tree->child[1]->kind.stmt == ReturnK) {
                            // Emit return on the same line as label
                            if (tree->child[1]->child[0] != NULL) {
                                char *returnValue = generateExpression(tree->child[1]->child[0]);
                                emitLabelReturn(label1, returnValue);
                            } else {
                                emitLabelReturn(label1, NULL);
                            }
                        } else {
                            emitLabel(label1);
                            generateStatement(tree->child[1]); // then part
                        }
                    } else {
                        // if only: negate condition and jump to end if true (skip then block)
                        if (tree->child[0]->kind.exp == OpK) {
                            // Don't call generateExpression twice on the same operands
                            // Use the condition components that were already generated above
                            char *left, *right;
                            if (tree->child[0]->child[0]->kind.exp == IdK && tree->child[0]->child[0]->child[0] != NULL) {
                                // For array access, we need to extract the temporary from the condition
                                // Parse the already generated condition to get the left operand
                                char *spacePos = strchr(condition, ' ');
                                if (spacePos != NULL) {
                                    int leftLen = spacePos - condition;
                                    left = (char *)malloc(leftLen + 1);
                                    strncpy(left, condition, leftLen);
                                    left[leftLen] = '\0';
                                } else {
                                    left = generateExpression(tree->child[0]->child[0]);
                                }
                            } else {
                                left = generateExpression(tree->child[0]->child[0]);
                            }
                            right = generateExpression(tree->child[0]->child[1]);
                            char *op = getOpString(tree->child[0]->attr.opr);
                            
                            // Negate the condition
                            char *negatedOp;
                            if (strcmp(op, "<") == 0) negatedOp = ">=";
                            else if (strcmp(op, "<=") == 0) negatedOp = ">";
                            else if (strcmp(op, ">") == 0) negatedOp = "<=";
                            else if (strcmp(op, ">=") == 0) negatedOp = "<";
                            else if (strcmp(op, "==") == 0) negatedOp = "!=";
                            else if (strcmp(op, "!=") == 0) negatedOp = "==";
                            else negatedOp = op;
                            
                            char *negatedCondition = (char *)malloc(100);
                            snprintf(negatedCondition, 100, "%s %s %s", left, negatedOp, right);
                            emitCondGoto(negatedCondition, label1);
                        } else {
                            // For non-comparison conditions, use as-is (this may need adjustment)
                            emitCondGoto(condition, label1);
                        }
                        generateStatement(tree->child[1]);
                        emitLabel(label1);
                    }
                    break;
                    
                case WhileK:
                    label1 = newLabel();
                    label2 = newLabel();
                    
                    // Handle condition like in if statements
                    if (tree->child[0]->kind.exp == OpK) {
                        // Generate condition directly without temporary
                        char *left = generateExpression(tree->child[0]->child[0]);
                        char *right = generateExpression(tree->child[0]->child[1]);
                        char *op = getOpString(tree->child[0]->attr.opr);
                        
                        // Negate the condition for while loop (if !condition goto end)
                        char *negatedOp;
                        if (strcmp(op, "<") == 0) negatedOp = ">=";
                        else if (strcmp(op, "<=") == 0) negatedOp = ">";
                        else if (strcmp(op, ">") == 0) negatedOp = "<=";
                        else if (strcmp(op, ">=") == 0) negatedOp = "<";
                        else if (strcmp(op, "==") == 0) negatedOp = "!=";
                        else if (strcmp(op, "!=") == 0) negatedOp = "==";
                        else negatedOp = op;
                        
                        condition = (char *)malloc(100);
                        snprintf(condition, 100, "%s %s %s", left, negatedOp, right);
                        emitLabelCondGoto(label1, condition, label2);
                    } else {
                        condition = generateExpression(tree->child[0]);
                        emitLabelCondGoto(label1, condition, label2);
                    }
                    
                    generateStatement(tree->child[1]);
                    emitGoto(label1);
                    emitLabel(label2);
                    break;
                    
                case ReturnK:
                    if (tree->child[0] != NULL) {
                        value = generateExpression(tree->child[0]);
                        emitReturn(value);
                    } else {
                        emitReturnVoid();
                    }
                    break;
                    
                default:
                    break;
            }
            break;
            
        case ExpK:
            switch (tree->kind.exp) {
                case CallK:
                    // Check if this is a void function call (like sort, output)
                    if (strcmp(tree->attr.name, "output") == 0 || strcmp(tree->attr.name, "sort") == 0) {
                        emitCallVoid(tree->attr.name, tree->child[0]);
                    } else {
                        char *temp = newTemp();
                        emitCall(temp, tree->attr.name, tree->child[0]);
                    }
                    break;
                    
                default:
                    break;
            }
            break;
            
        default:
            break;
    }
    
    // Process siblings
    if (tree->sibling != NULL) {
        generateStatement(tree->sibling);
    }
}

// Generate code for functions
void generateFunction(TreeNode *tree) {
    if (tree == NULL || tree->kind.exp != FuncK) return;
    
    emitFuncHeader(tree->attr.name, tree->child[0]);
    
    // Generate function body (compound statement)
    if (tree->child[1] != NULL) {
        generateStatement(tree->child[1]);
    }
    
    // Add implicit return for void functions if not already present
    if (tree->type == voidDType) {
        emitReturnVoid();
    }
    
    emitFuncEnd();
}

// First pass: collect global variable declarations
void generateGlobalDeclarations(TreeNode *tree) {
    if (tree == NULL) return;
    
    switch (tree->nodekind) {
        case ExpK:
            switch (tree->kind.exp) {
                case TypeK:
                    if (tree->child[0] != NULL && tree->child[0]->kind.exp == VarK) {
                        // Global variable declaration (not inside a function)
                        if (tree->child[0]->child[0] != NULL) {
                            // Array
                            addGlobalVar(tree->child[0]->attr.name, tree->child[0]->child[0]->attr.val);
                            emitGlobalVar(tree->child[0]->attr.name, tree->child[0]->child[0]->attr.val);
                        } else {
                            // Simple variable
                            addGlobalVar(tree->child[0]->attr.name, 1);
                            emitGlobalVar(tree->child[0]->attr.name, 1);
                        }
                    }
                    // Skip function declarations in this pass
                    break;
                    
                default:
                    break;
            }
            break;
            
        default:
            break;
    }
    
    // Only process siblings at the top level (global scope)
    generateGlobalDeclarations(tree->sibling);
}

// Second pass: generate function code
void generateFunctions(TreeNode *tree) {
    static int firstFunction = 1;
    if (tree == NULL) return;
    
    switch (tree->nodekind) {
        case ExpK:
            switch (tree->kind.exp) {
                case TypeK:
                    if (tree->child[0] != NULL && tree->child[0]->kind.exp == FuncK) {
                        if (!firstFunction) {
                            fprintf(outputFile, "\n"); // Add blank line before function (except first)
                        }
                        firstFunction = 0;
                        generateFunction(tree->child[0]);
                    }
                    break;
                    
                default:
                    break;
            }
            break;
            
        default:
            break;
    }
    
    // Process siblings
    generateFunctions(tree->sibling);
}

// Main code generation function
void codeGen(TreeNode *syntaxTree) {
    // Open output file
    outputFile = fopen("output.ir", "w");
    if (outputFile == NULL) {
        fprintf(stderr, "Error: Could not open output.ir for writing\n");
        return;
    }
    
    // First pass: Generate global variable declarations
    generateGlobalDeclarations(syntaxTree);
    
    // Add blank line after global declarations
    fprintf(outputFile, "\n");
    
    // Second pass: Generate function code
    generateFunctions(syntaxTree);
    
    // Close output file
    fclose(outputFile);
}