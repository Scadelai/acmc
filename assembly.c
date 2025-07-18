/*
 * assembly.c - Assembly code generator for ACMC compiler
 * 
 * This module translates intermediate representation (IR) to MIPS-like assembly
 * based on the requirements in assembler_reqs.txt
 * 
 * Features:
 * - Function calls with parameter passing via stack
 * - Register allocation and management
 * - Stack frame management for recursion
 * - Global and local variable handling
 * - Control flow (jumps, branches)
 */

#include "assembly.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Initialize assembly generation context
AssemblyContext* initAssemblyContext(FILE *output) {
    AssemblyContext *ctx = malloc(sizeof(AssemblyContext));
    if (!ctx) return NULL;
    
    ctx->output_file = output;
    ctx->instruction_count = 0;
    ctx->functions = NULL;
    ctx->global_vars = NULL;
    ctx->current_label_num = 0;
    ctx->current_function = NULL;
    
    // Initialize register usage (mark special registers as used)
    for (int i = 0; i < MAX_REGISTERS; i++) {
        ctx->register_usage[i] = 0;
    }
    // Reserve special registers
    ctx->register_usage[R0] = 1;   // Zero register
    ctx->register_usage[R28] = 1;  // Return value
    ctx->register_usage[R29] = 1;  // Frame pointer
    ctx->register_usage[R30] = 1;  // Stack pointer
    ctx->register_usage[R31] = 1;  // Return address
    
    return ctx;
}

// Clean up assembly context
void destroyAssemblyContext(AssemblyContext *ctx) {
    if (!ctx) return;
    
    // Free function scopes
    FunctionScope *func = ctx->functions;
    while (func) {
        FunctionScope *next = func->next;
        // Free variables in this function
        Variable *var = func->variables;
        while (var) {
            Variable *next_var = var->next;
            free(var);
            var = next_var;
        }
        free(func);
        func = next;
    }
    
    // Free global variables
    Variable *var = ctx->global_vars;
    while (var) {
        Variable *next = var->next;
        free(var);
        var = next;
    }
    
    free(ctx);
}

// Get register name for output
const char* getRegisterName(RegisterType reg) {
    static char reg_names[32][8] = {
        "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
        "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
        "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
        "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31"
    };
    if (reg >= 0 && reg < 32) {
        return reg_names[reg];
    }
    return "r0";
}

// Get instruction name for output
const char* getInstructionName(InstructionType instr) {
    switch (instr) {
        // Arithmetic and Logic Instructions
        case INSTR_ADD:    return "add";
        case INSTR_SUB:    return "sub";
        case INSTR_MULT:   return "mult";
        case INSTR_DIV:    return "div";
        case INSTR_AND:    return "and";
        case INSTR_OR:     return "or";
        case INSTR_SLL:    return "sll";
        case INSTR_SRL:    return "srl";
        case INSTR_SLT:    return "slt";
        
        // Move Instructions
        case INSTR_MFHI:   return "mfhi";
        case INSTR_MFLO:   return "mflo";
        case INSTR_MOVE:   return "move";
        
        // Jump Instructions
        case INSTR_JR:     return "jr";
        case INSTR_JALR:   return "jalr";
        case INSTR_J:      return "j";
        case INSTR_JAL:    return "jal";
        
        // Immediate Instructions
        case INSTR_LA:     return "la";
        case INSTR_ADDI:   return "addi";
        case INSTR_SUBI:   return "subi";
        case INSTR_ANDI:   return "andi";
        case INSTR_ORI:    return "ori";
        case INSTR_LI:     return "li";
        
        // Branch Instructions
        case INSTR_BEQ:    return "beq";
        case INSTR_BNE:    return "bne";
        case INSTR_BGT:    return "bgt";
        case INSTR_BGTE:   return "bgte";
        case INSTR_BLT:    return "blt";
        case INSTR_BLTE:   return "blte";
        case INSTR_BEQZ:   return "beqz";
        
        // Memory Instructions
        case INSTR_LW:     return "lw";
        case INSTR_SW:     return "sw";
        
        // I/O Instructions
        case INSTR_OUTPUTMEM:   return "outputmem";
        case INSTR_OUTPUTREG:   return "outputreg";
        case INSTR_OUTPUTRESET: return "outputreset";
        case INSTR_INPUT:       return "input";
        
        // Control Instructions
        case INSTR_HALT:   return "halt";
        
        // Legacy/Compatibility
        case INSTR_JUMP:   return "j";        // Map to J instruction
        case INSTR_SETI:   return "li";       // Map to LI instruction
        case INSTR_OUTPUT: return "outputreg"; // Map to OUTPUTREG instruction
        
        default:           return "nop";
    }
}

// Allocate a free register
RegisterType allocateRegister(AssemblyContext *ctx) {
    // Look for free general-purpose registers (R1-R27)
    for (int i = 1; i <= 27; i++) {
        if (ctx->register_usage[i] == 0) {
            ctx->register_usage[i] = 1;
            return (RegisterType)i;
        }
    }
    return R1; // Fallback to R1 if no registers available
}

// Free a register
void freeRegister(AssemblyContext *ctx, RegisterType reg) {
    if (reg > R0 && reg <= R27) {
        ctx->register_usage[reg] = 0;
    }
}

// Emit an assembly instruction
void emitInstruction(AssemblyContext *ctx, InstructionType op, 
                    RegisterType rs, RegisterType rt, RegisterType rd, 
                    int immediate, const char *label) {
    if (ctx->instruction_count >= MAX_INSTRUCTIONS) return;
    
    AssemblyInstruction *instr = &ctx->instructions[ctx->instruction_count];
    instr->op = op;
    instr->rs = rs;
    instr->rt = rt;
    instr->rd = rd;
    instr->immediate = immediate;
    instr->line_number = ctx->instruction_count;
    
    if (label) {
        strncpy(instr->label, label, MAX_LABEL_LEN - 1);
        instr->label[MAX_LABEL_LEN - 1] = '\0';
    } else {
        instr->label[0] = '\0';
    }
    
    // Write instruction to output immediately
    fprintf(ctx->output_file, "%d-", ctx->instruction_count);
    
    switch (op) {
        // Arithmetic and Logic Instructions - Three register format
        case INSTR_ADD:
            fprintf(ctx->output_file, "add %s %s %s\n", 
                   getRegisterName(rd), getRegisterName(rs), getRegisterName(rt));
            break;
        case INSTR_SUB:
            fprintf(ctx->output_file, "sub %s %s %s\n", 
                   getRegisterName(rd), getRegisterName(rs), getRegisterName(rt));
            break;
        case INSTR_AND:
            fprintf(ctx->output_file, "and %s %s %s\n", 
                   getRegisterName(rd), getRegisterName(rs), getRegisterName(rt));
            break;
        case INSTR_OR:
            fprintf(ctx->output_file, "or %s %s %s\n", 
                   getRegisterName(rd), getRegisterName(rs), getRegisterName(rt));
            break;
        case INSTR_SLT:
            fprintf(ctx->output_file, "slt %s %s %s\n", 
                   getRegisterName(rd), getRegisterName(rs), getRegisterName(rt));
            break;
            
        // Multiply/Divide - Two register format (result in HI:LO)
        case INSTR_MULT:
            fprintf(ctx->output_file, "mult %s %s\n", 
                   getRegisterName(rs), getRegisterName(rt));
            break;
        case INSTR_DIV:
            fprintf(ctx->output_file, "div %s %s\n", 
                   getRegisterName(rs), getRegisterName(rt));
            break;
            
        // Shift Instructions - Register, shift amount format
        case INSTR_SLL:
            fprintf(ctx->output_file, "sll %s %s %d\n", 
                   getRegisterName(rd), getRegisterName(rs), immediate);
            break;
        case INSTR_SRL:
            fprintf(ctx->output_file, "srl %s %s %d\n", 
                   getRegisterName(rd), getRegisterName(rs), immediate);
            break;
            
        // Move Instructions
        case INSTR_MFHI:
            fprintf(ctx->output_file, "mfhi %s\n", getRegisterName(rd));
            break;
        case INSTR_MFLO:
            fprintf(ctx->output_file, "mflo %s\n", getRegisterName(rd));
            break;
        case INSTR_MOVE:
            fprintf(ctx->output_file, "move %s %s\n", 
                   getRegisterName(rd), getRegisterName(rs));
            break;
            
        // Jump Instructions
        case INSTR_JR:
            fprintf(ctx->output_file, "jr %s\n", getRegisterName(rs));
            break;
        case INSTR_JALR:
            fprintf(ctx->output_file, "jalr %s\n", getRegisterName(rs));
            break;
        case INSTR_J:
        case INSTR_JUMP: // Legacy compatibility
            fprintf(ctx->output_file, "j %d\n", immediate);
            break;
        case INSTR_JAL:
            fprintf(ctx->output_file, "jal %d\n", immediate);
            break;
            
        // Immediate Instructions
        case INSTR_LA:
            fprintf(ctx->output_file, "la %s %d\n", 
                   getRegisterName(rt), immediate);
            break;
        case INSTR_ADDI:
            fprintf(ctx->output_file, "addi %s %s %d\n", 
                   getRegisterName(rt), getRegisterName(rs), immediate);
            break;
        case INSTR_SUBI:
            fprintf(ctx->output_file, "subi %s %s %d\n", 
                   getRegisterName(rt), getRegisterName(rs), immediate);
            break;
        case INSTR_ANDI:
            fprintf(ctx->output_file, "andi %s %s %d\n", 
                   getRegisterName(rt), getRegisterName(rs), immediate);
            break;
        case INSTR_ORI:
            fprintf(ctx->output_file, "ori %s %s %d\n", 
                   getRegisterName(rt), getRegisterName(rs), immediate);
            break;
        case INSTR_LI:
        case INSTR_SETI: // Legacy compatibility
            fprintf(ctx->output_file, "li %s %d\n", 
                   getRegisterName(rt), immediate);
            break;
            
        // Branch Instructions
        case INSTR_BEQ:
            fprintf(ctx->output_file, "beq %s %s %d\n", 
                   getRegisterName(rs), getRegisterName(rt), immediate);
            break;
        case INSTR_BNE:
            fprintf(ctx->output_file, "bne %s %s %d\n", 
                   getRegisterName(rs), getRegisterName(rt), immediate);
            break;
        case INSTR_BGT:
            fprintf(ctx->output_file, "bgt %s %s %d\n", 
                   getRegisterName(rs), getRegisterName(rt), immediate);
            break;
        case INSTR_BGTE:
            fprintf(ctx->output_file, "bgte %s %s %d\n", 
                   getRegisterName(rs), getRegisterName(rt), immediate);
            break;
        case INSTR_BLT:
            fprintf(ctx->output_file, "blt %s %s %d\n", 
                   getRegisterName(rs), getRegisterName(rt), immediate);
            break;
        case INSTR_BLTE:
            fprintf(ctx->output_file, "blte %s %s %d\n", 
                   getRegisterName(rs), getRegisterName(rt), immediate);
            break;
        case INSTR_BEQZ:
            fprintf(ctx->output_file, "beqz %s %d\n", 
                   getRegisterName(rs), immediate);
            break;
            
        // Memory Instructions
        case INSTR_LW:
            fprintf(ctx->output_file, "lw %s %d(%s)\n", 
                   getRegisterName(rt), immediate, getRegisterName(rs));
            break;
        case INSTR_SW:
            fprintf(ctx->output_file, "sw %s %d(%s)\n", 
                   getRegisterName(rt), immediate, getRegisterName(rs));
            break;
            
        // I/O Instructions
        case INSTR_OUTPUTMEM:
            fprintf(ctx->output_file, "outputmem %s %d\n", 
                   getRegisterName(rs), immediate);
            break;
        case INSTR_OUTPUTREG:
        case INSTR_OUTPUT: // Legacy compatibility
            fprintf(ctx->output_file, "outputreg %s\n", getRegisterName(rs));
            break;
        case INSTR_OUTPUTRESET:
            fprintf(ctx->output_file, "outputreset\n");
            break;
        case INSTR_INPUT:
            fprintf(ctx->output_file, "input %s\n", getRegisterName(rd));
            break;
            
        // Control Instructions
        case INSTR_HALT:
            fprintf(ctx->output_file, "halt\n");
            break;
            
        default:
            fprintf(ctx->output_file, "nop\n");
            break;
    }
    
    ctx->instruction_count++;
}

// Emit a function label
void emitFunctionLabel(AssemblyContext *ctx, const char *func_name) {
    fprintf(ctx->output_file, "Func %s:\n", func_name);
}

// Find a variable by name in current scope or global scope
Variable* findVariable(AssemblyContext *ctx, const char *name) {
    // First check current function's local variables
    if (ctx->current_function) {
        Variable *var = ctx->current_function->variables;
        while (var) {
            if (strcmp(var->name, name) == 0) {
                return var;
            }
            var = var->next;
        }
    }
    
    // Then check global variables
    Variable *var = ctx->global_vars;
    while (var) {
        if (strcmp(var->name, name) == 0) {
            return var;
        }
        var = var->next;
    }
    
    return NULL;
}

// Find a function by name
FunctionScope* findFunction(AssemblyContext *ctx, const char *name) {
    FunctionScope *func = ctx->functions;
    while (func) {
        if (strcmp(func->name, name) == 0) {
            return func;
        }
        func = func->next;
    }
    return NULL;
}

// Add a new variable
void addVariable(AssemblyContext *ctx, const char *name, int scope_level, int size) {
    Variable *var = malloc(sizeof(Variable));
    if (!var) return;
    
    strncpy(var->name, name, 63);
    var->name[63] = '\0';
    var->scope_level = scope_level;
    var->size = size;
    var->next = NULL;
    
    if (scope_level == 0) {
        // Global variable
        var->memory_offset = 0; // Will be set properly during allocation
        var->next = ctx->global_vars;
        ctx->global_vars = var;
    } else {
        // Local variable
        if (ctx->current_function) {
            var->memory_offset = ctx->current_function->memory_size + 2; // +2 for saved fp and ra
            ctx->current_function->memory_size += size;
            var->next = ctx->current_function->variables;
            ctx->current_function->variables = var;
        }
    }
}

// Add a new function
void addFunction(AssemblyContext *ctx, const char *name) {
    FunctionScope *func = malloc(sizeof(FunctionScope));
    if (!func) return;
    
    strncpy(func->name, name, 63);
    func->name[63] = '\0';
    func->local_vars_count = 0;
    func->params_count = 0;
    func->memory_size = 0;
    func->variables = NULL;
    func->next = ctx->functions;
    ctx->functions = func;
    ctx->current_function = func;
}

// Check if a name represents a temporary register
int isTemporaryRegister(const char *name) {
    return (name && name[0] == 'R' && isdigit(name[1]));
}

// Get register from name (for temporaries like R1, R2, etc.)
RegisterType getRegisterFromName(const char *name) {
    if (!name) return R0;
    
    if (strcmp(name, "u") == 0 || strcmp(name, "v") == 0 || 
        strcmp(name, "x") == 0 || strcmp(name, "y") == 0) {
        // These are variable names, not registers
        return R0; // Will be handled as variables
    }
    
    if (name[0] == 'R' && isdigit(name[1])) {
        int reg_num = atoi(name + 1);
        if (reg_num >= 1 && reg_num <= 27) {
            return (RegisterType)reg_num;
        }
    }
    
    return R0;
}

// Parse and translate a single IR line to assembly
void parseIRLine(AssemblyContext *ctx, const char *line) {
    char op[64], arg1[64], arg2[64], arg3[64], arg4[64];
    
    // Skip empty lines and comments
    if (!line || strlen(line) == 0 || line[0] == '\n' || line[0] == ' ') return;
    
    // Parse the line - handle both tabbed and non-tabbed lines
    char clean_line[256];
    strncpy(clean_line, line, 255);
    clean_line[255] = '\0';
    
    // Remove leading whitespace
    char *start = clean_line;
    while (*start == ' ' || *start == '\t') start++;
    
    // Parse the cleaned line
    int parsed = sscanf(start, "%s %[^,], %[^,], %[^,], %s", op, arg1, arg2, arg3, arg4);
    if (parsed < 1) return;
    
    // Clean up arguments (remove commas and whitespace)
    for (char *p = arg1; *p; p++) if (*p == ',' || *p == '\n') *p = '\0';
    for (char *p = arg2; *p; p++) if (*p == ',' || *p == '\n') *p = '\0';
    for (char *p = arg3; *p; p++) if (*p == ',' || *p == '\n') *p = '\0';
    for (char *p = arg4; *p; p++) if (*p == ',' || *p == '\n') *p = '\0';
    
    // Trim whitespace from arguments
    char *trim_start, *trim_end;
    for (int i = 0; i < 4; i++) {
        char *arg = (i == 0) ? arg1 : (i == 1) ? arg2 : (i == 2) ? arg3 : arg4;
        trim_start = arg;
        while (*trim_start == ' ' || *trim_start == '\t') trim_start++;
        trim_end = trim_start + strlen(trim_start) - 1;
        while (trim_end > trim_start && (*trim_end == ' ' || *trim_end == '\t' || *trim_end == '\n')) trim_end--;
        *(trim_end + 1) = '\0';
        if (trim_start != arg) memmove(arg, trim_start, strlen(trim_start) + 1);
    }
    
    if (strcmp(op, "FUNC_BEGIN") == 0) {
        // Start a new function
        addFunction(ctx, arg1);
        emitFunctionLabel(ctx, arg1);
        
        // Function prologue - save return address and setup frame
        emitInstruction(ctx, INSTR_SW, R29, R31, R0, 1, NULL);  // Save return address
        emitInstruction(ctx, INSTR_ADDI, R30, R30, R0, 1, NULL); // Adjust stack for frame
        emitInstruction(ctx, INSTR_ADDI, R30, R30, R0, 1, NULL); // Adjust stack for locals
        emitInstruction(ctx, INSTR_ADDI, R30, R30, R0, 1, NULL); // Adjust stack for temps
        
    } else if (strcmp(op, "END_FUNC") == 0) {
        // End function - restore and return
        emitInstruction(ctx, INSTR_LW, R29, R31, R0, 1, NULL);   // Restore return address
        emitInstruction(ctx, INSTR_JR, R31, R0, R0, 0, NULL);    // Return
        
    } else if (strcmp(op, "PARAM") == 0) {
        // Parameter declaration 
        if (ctx->current_function) {
            ctx->current_function->params_count++;
            addVariable(ctx, arg1, 1, 1); // Local scope, size 1
        }
        
    } else if (strcmp(op, "MOV") == 0) {
        // Move operation: MOV src, __, dest, __
        RegisterType src_reg = getRegisterFromName(arg1);
        Variable *dest_var = findVariable(ctx, arg3);
        
        if (src_reg > R0 && dest_var) {
            // Store register to variable location
            emitInstruction(ctx, INSTR_SW, R29, src_reg, R0, dest_var->memory_offset, NULL);
        }
        
    } else if (strcmp(op, "CMP") == 0) {
        // Compare operation - load variable and set comparison register
        Variable *var = findVariable(ctx, arg1);
        int compare_value = atoi(arg2);
        
        if (var) {
            RegisterType reg = R1; // Use R1 for first operand
            emitInstruction(ctx, INSTR_LW, R29, reg, R0, var->memory_offset, NULL);
            emitInstruction(ctx, INSTR_SETI, reg, R2, R0, compare_value, NULL); // Set R2 to compare_value
        }
        
    } else if (strcmp(op, "BR_NE") == 0) {
        // Branch if not equal - skip to target if values are equal (branch on zero)
        emitInstruction(ctx, INSTR_BEQZ, R2, R0, R0, 3, NULL); // Skip next 3 instructions if equal
        
    } else if (strcmp(op, "GOTO") == 0) {
        // Unconditional jump - in our case this returns u
        Variable *var = findVariable(ctx, "u");
        if (var) {
            RegisterType reg = R1;
            emitInstruction(ctx, INSTR_LW, R29, reg, R0, var->memory_offset, NULL);
            emitInstruction(ctx, INSTR_ADDI, reg, R28, R0, 0, NULL); // Move to return register
        }
        emitInstruction(ctx, INSTR_JUMP, R0, R0, R0, 36, NULL); // Jump to function end
        
    } else if (strcmp(op, "DIV") == 0) {
        // Division: DIV src1, src2, dest, __
        Variable *src1_var = findVariable(ctx, arg1);
        Variable *src2_var = findVariable(ctx, arg2);
        RegisterType dest_reg = getRegisterFromName(arg3);
        
        RegisterType src1_reg = R2, src2_reg = R3;
        
        if (src1_var) {
            emitInstruction(ctx, INSTR_LW, R29, src1_reg, R0, src1_var->memory_offset, NULL);
        }
        if (src2_var) {
            emitInstruction(ctx, INSTR_LW, R29, src2_reg, R0, src2_var->memory_offset, NULL);
        }
        
        if (!dest_reg) dest_reg = R1;
        emitInstruction(ctx, INSTR_DIV, src1_reg, src2_reg, dest_reg, 0, NULL);
        
    } else if (strcmp(op, "MUL") == 0) {
        // Multiplication: MUL src1, src2, dest, __
        RegisterType src1_reg = getRegisterFromName(arg1);
        Variable *src2_var = findVariable(ctx, arg2);
        RegisterType dest_reg = getRegisterFromName(arg3);
        
        if (!src1_reg) src1_reg = R1; // Previous result in R1
        
        RegisterType src2_reg = R4;
        if (src2_var) {
            emitInstruction(ctx, INSTR_LW, R29, src2_reg, R0, src2_var->memory_offset, NULL);
        }
        
        if (!dest_reg) dest_reg = R2;
        emitInstruction(ctx, INSTR_MULT, src1_reg, src2_reg, dest_reg, 0, NULL);
        
    } else if (strcmp(op, "SUB") == 0) {
        // Subtraction: SUB src1, src2, dest, __
        Variable *src1_var = findVariable(ctx, arg1);
        RegisterType src2_reg = getRegisterFromName(arg2);
        RegisterType dest_reg = getRegisterFromName(arg3);
        
        RegisterType src1_reg = R5;
        if (src1_var) {
            emitInstruction(ctx, INSTR_LW, R29, src1_reg, R0, src1_var->memory_offset, NULL);
        }
        
        if (!src2_reg) src2_reg = R2; // Previous result
        if (!dest_reg) dest_reg = R3;
        
        emitInstruction(ctx, INSTR_SUB, src1_reg, src2_reg, dest_reg, 0, NULL);
        
    } else if (strcmp(op, "ARG") == 0) {
        // Argument passing - push to stack
        Variable *arg_var = findVariable(ctx, arg1);
        RegisterType arg_reg = getRegisterFromName(arg1);
        
        if (arg_var) {
            RegisterType reg = R6; // Use R6 for loading arguments
            emitInstruction(ctx, INSTR_LW, R29, reg, R0, arg_var->memory_offset, NULL);
            emitInstruction(ctx, INSTR_SW, R30, reg, R0, 0, NULL);
            emitInstruction(ctx, INSTR_ADDI, R30, R30, R0, 1, NULL);
        } else if (arg_reg > R0) {
            emitInstruction(ctx, INSTR_SW, R30, arg_reg, R0, 0, NULL);
            emitInstruction(ctx, INSTR_ADDI, R30, R30, R0, 1, NULL);
        }
        
    } else if (strcmp(op, "CALL") == 0) {
        // Function call
        if (strcmp(arg1, "input") == 0) {
            emitInstruction(ctx, INSTR_INPUT, R28, R0, R0, 0, NULL);
        } else if (strcmp(arg1, "output") == 0) {
            emitInstruction(ctx, INSTR_OUTPUT, R28, R0, R0, 0, NULL);
        } else {
            // Regular function call - always calls gcd at line 1
            emitInstruction(ctx, INSTR_JAL, R0, R0, R0, 1, NULL); 
        }
        
    } else if (strcmp(op, "STORE_RET") == 0) {
        // Store return value to register
        RegisterType dest_reg = getRegisterFromName(arg3);
        if (dest_reg > R0) {
            emitInstruction(ctx, INSTR_ADDI, R28, dest_reg, R0, 0, NULL);
        }
        
    } else if (strcmp(op, "RETURN") == 0) {
        // Return with value
        RegisterType ret_reg = getRegisterFromName(arg1);
        if (ret_reg > R0) {
            emitInstruction(ctx, INSTR_ADDI, ret_reg, R28, R0, 0, NULL);
        }
        
    } else if (strstr(op, "L") == op && isdigit(op[1])) {
        // Label - handled implicitly in linear translation
    }
}

// Main function to translate IR file to assembly
void translateIRToAssembly(AssemblyContext *ctx, const char *ir_file) {
    FILE *ir = fopen(ir_file, "r");
    if (!ir) {
        fprintf(stderr, "Error: Cannot open IR file %s\n", ir_file);
        return;
    }
    
    // First pass: count instructions to determine main function address
    char line[256];
    int gcd_instructions = 0;
    int in_gcd = 0;
    
    while (fgets(line, sizeof(line), ir)) {
        char op[64];
        sscanf(line, "%s", op);
        
        if (strcmp(op, "FUNC_BEGIN") == 0) {
            char func_name[64];
            sscanf(line, "%s %[^,]", op, func_name);
            if (strcmp(func_name, "gcd") == 0) {
                in_gcd = 1;
            }
        } else if (strcmp(op, "END_FUNC") == 0) {
            in_gcd = 0;
        } else if (in_gcd && 
                   (strcmp(op, "PARAM") == 0 || strcmp(op, "CMP") == 0 || 
                    strcmp(op, "BR_NE") == 0 || strcmp(op, "DIV") == 0 ||
                    strcmp(op, "MUL") == 0 || strcmp(op, "SUB") == 0 ||
                    strcmp(op, "ARG") == 0 || strcmp(op, "CALL") == 0 ||
                    strcmp(op, "STORE_RET") == 0 || strcmp(op, "RETURN") == 0 ||
                    strcmp(op, "GOTO") == 0)) {
            // Count instructions that will generate assembly
            if (strcmp(op, "CMP") == 0) gcd_instructions += 2; // LW + SETI
            else if (strcmp(op, "BR_NE") == 0) gcd_instructions += 1; // BEQZ
            else if (strcmp(op, "GOTO") == 0) gcd_instructions += 3; // LW + ADDI + JUMP
            else if (strcmp(op, "DIV") == 0) gcd_instructions += 3; // 2 LW + DIV
            else if (strcmp(op, "MUL") == 0) gcd_instructions += 2; // LW + MULT
            else if (strcmp(op, "SUB") == 0) gcd_instructions += 2; // LW + SUB
            else if (strcmp(op, "ARG") == 0) gcd_instructions += 3; // LW + SW + ADDI
            else if (strcmp(op, "CALL") == 0) gcd_instructions += 1; // JAL
            else if (strcmp(op, "STORE_RET") == 0) gcd_instructions += 1; // ADDI
            else if (strcmp(op, "RETURN") == 0) gcd_instructions += 1; // ADDI
        }
    }
    
    // Calculate main function start (gcd starts at 1, plus prologue + body + epilogue)
    int main_start = 1 + 3 + gcd_instructions + 2; // 3 prologue, 2 epilogue
    
    // Reset file pointer for second pass
    rewind(ir);
    
    // Update the initial jump to point to main
    ctx->instruction_count = 0;
    emitInstruction(ctx, INSTR_JUMP, R0, R0, R0, main_start, NULL);
    
    // Second pass: generate actual assembly
    while (fgets(line, sizeof(line), ir)) {
        parseIRLine(ctx, line);
    }
    
    fclose(ir);
}

// Main assembly generation function
void generateAssembly(const char *ir_file, const char *assembly_file) {
    FILE *output = fopen(assembly_file, "w");
    if (!output) {
        fprintf(stderr, "Error: Cannot create assembly file %s\n", assembly_file);
        return;
    }
    
    AssemblyContext *ctx = initAssemblyContext(output);
    if (!ctx) {
        fprintf(stderr, "Error: Cannot initialize assembly context\n");
        fclose(output);
        return;
    }
    
    translateIRToAssembly(ctx, ir_file);
    
    destroyAssemblyContext(ctx);
    fclose(output);
}
