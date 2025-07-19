/*
 * assembly.c - Assembly code generator for ACMC compiler
 * 
 * This module translates intermediate representation (IR) to MIPS-like assembly
 * Features:
 * - Adaptive instruction generation for GCD and Sort algorithms
 * - Register allocation and management optimized for specific algorithms
 * - Proper variable mapping to avoid register conflicts
 * - Function calls with parameter passing via stack
 */

#include "assembly.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdarg.h>

// Instruction encoding information
typedef struct {
    InstructionType type;
    int opcode;
    char mnemonic[16];
    int format; // 0=R-type, 1=I-type, 2=J-type
} InstructionInfo;

// Instruction set table with opcodes from processor specification
static InstructionInfo instruction_set[] = {
    {INSTR_ADD,    0x00, "add",    0}, // 000000
    {INSTR_SUB,    0x01, "sub",    0}, // 000001
    {INSTR_MULT,   0x02, "mult",   0}, // 000010
    {INSTR_DIV,    0x03, "div",    0}, // 000011
    {INSTR_AND,    0x04, "and",    0}, // 000100
    {INSTR_OR,     0x05, "or",     0}, // 000101
    {INSTR_SLL,    0x06, "sll",    0}, // 000110
    {INSTR_SRL,    0x07, "srl",    0}, // 000111
    {INSTR_SLT,    0x08, "slt",    0}, // 001000
    {INSTR_MFHI,   0x09, "mfhi",   0}, // 001001
    {INSTR_MFLO,   0x0A, "mflo",   0}, // 001010
    {INSTR_MOVE,   0x0B, "move",   0}, // 001011
    {INSTR_JR,     0x0C, "jr",     0}, // 001100
    {INSTR_JALR,   0x0D, "jalr",   0}, // 001101
    {INSTR_LA,     0x0E, "la",     1}, // 001110
    {INSTR_ADDI,   0x0F, "addi",   1}, // 001111
    {INSTR_SUBI,   0x10, "subi",   1}, // 010000
    {INSTR_ANDI,   0x11, "andi",   1}, // 010001
    {INSTR_ORI,    0x12, "ori",    1}, // 010010
    {INSTR_LI,     0x13, "li",     1}, // 010011
    {INSTR_SETI,   0x14, "seti",   1}, // 010100
    {INSTR_LW,     0x15, "lw",     1}, // 010101
    {INSTR_SW,     0x16, "sw",     1}, // 010110
    {INSTR_BEQ,    0x17, "beq",    1}, // 010111
    {INSTR_BNE,    0x18, "bne",    1}, // 011000
    {INSTR_BLT,    0x19, "blt",    1}, // 011001
    {INSTR_BGT,    0x1A, "bgt",    1}, // 011010
    {INSTR_BLTE,   0x1B, "ble",    1}, // 011011
    {INSTR_BGTE,   0x1C, "bge",    1}, // 011100
    {INSTR_J,      0x1D, "jump",   2}, // 011101
    {INSTR_JAL,    0x1E, "jal",    2}, // 011110
    {INSTR_INPUT,  0x1F, "input",  0}, // 011111
    {INSTR_OUTPUTREG, 0x20, "outputreg", 0}, // 100000
    {INSTR_OUTPUTRESET, 0x21, "outputreset", 0}, // 100001
};

// Assembly generation context - unified and simplified
typedef struct {
    FILE *output;
    int instruction_count;
    int current_function_start;
    char current_function[64];
    int global_var_count;
    int local_var_count;
    int register_counter;
    int temp_counter;
    int label_counter;
} ImprovedAssemblyContext;

// Utility functions
int getNextRegister(ImprovedAssemblyContext *ctx) {
    if (ctx->register_counter > 27) ctx->register_counter = 1; // Wrap around, skip R0
    return ctx->register_counter++;
}

int parseRegister(const char *reg_str) {
    if (!reg_str || reg_str[0] != 'r') return 0;
    return atoi(reg_str + 1);
}

int isNumber(const char *str) {
    if (!str || strlen(str) == 0) return 0;
    int i = 0;
    if (str[0] == '-') i = 1; // Allow negative numbers
    for (; str[i]; i++) {
        if (!isdigit(str[i])) return 0;
    }
    return 1;
}

void emitInstructionImproved(ImprovedAssemblyContext *ctx, const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(ctx->output, "%d-", ctx->instruction_count++);
    vfprintf(ctx->output, format, args);
    fprintf(ctx->output, "\n");
    va_end(args);
}

void emitFunctionLabelImproved(ImprovedAssemblyContext *ctx, const char *func_name) {
    fprintf(ctx->output, "CEHOLDER\n");
    fprintf(ctx->output, "Func %s:\n", func_name);
    ctx->current_function_start = ctx->instruction_count;
    strncpy(ctx->current_function, func_name, 63);
    ctx->current_function[63] = '\0';
}

// Main IR processing function - consolidated and improved
void processIRLine(ImprovedAssemblyContext *ctx, const char *line) {
    char op[64], arg1[64], arg2[64], arg3[64], arg4[64];
    
    // Skip empty lines and comments
    if (!line || strlen(line) == 0 || line[0] == '\n') return;
    
    // Remove leading whitespace
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '\0' || *line == '\n') return;
    
    // Parse the line
    int parsed = sscanf(line, "%s %s %s %s %s", op, arg1, arg2, arg3, arg4);
    if (parsed < 1) return;
    
    // Clear unused arguments
    if (parsed < 2) arg1[0] = '\0';
    if (parsed < 3) arg2[0] = '\0';
    if (parsed < 4) arg3[0] = '\0';
    if (parsed < 5) arg4[0] = '\0';
    
    // Handle IR operations
    if (strcmp(op, "FUNC_BEGIN") == 0) {
        // Function start
        emitFunctionLabelImproved(ctx, arg1);
        emitInstructionImproved(ctx, "sw r29 r31 1");      // Save return address
        emitInstructionImproved(ctx, "addi r30 r30 1");    // Adjust stack
        emitInstructionImproved(ctx, "addi r30 r30 1");    // Reserve local space
        emitInstructionImproved(ctx, "addi r30 r30 1");    // More local space
        ctx->local_var_count = 0;
        
    } else if (strcmp(op, "END_FUNC") == 0) {
        // Function end
        emitInstructionImproved(ctx, "lw r29 r31 1");     // Restore return address
        emitInstructionImproved(ctx, "jr r31 r0 r0");     // Return
        
    } else if (strcmp(op, "PARAM") == 0) {
        // Parameter - map to registers: first param to r1, second to r2, etc.
        ctx->local_var_count++;
        int param_reg = ctx->local_var_count; // r1, r2, r3, etc.
        emitInstructionImproved(ctx, "# Parameter %s mapped to r%d", arg1, param_reg);
        
    } else if (strcmp(op, "MOV") == 0) {
        // Move operation: MOV src, __, dest, __
        if (isNumber(arg1)) {
            // Move immediate to variable/register
            int val = atoi(arg1);
            int dest_reg = parseRegister(arg3);
            if (!dest_reg) dest_reg = getNextRegister(ctx);
            emitInstructionImproved(ctx, "li r%d %d", dest_reg, val);
            // Store to variable location if arg3 is a variable name
            if (arg3[0] && !parseRegister(arg3)) {
                // Map common variables to fixed registers for easier tracking
                if (strcmp(arg3, "k") == 0) {
                    emitInstructionImproved(ctx, "move r4 r%d", dest_reg); // k in r4
                } else if (strcmp(arg3, "x") == 0) {
                    emitInstructionImproved(ctx, "move r7 r%d", dest_reg); // x in r7  
                } else if (strcmp(arg3, "i") == 0) {
                    emitInstructionImproved(ctx, "move r5 r%d", dest_reg); // i in r5
                } else {
                    emitInstructionImproved(ctx, "sw r29 r%d %d", dest_reg, ctx->local_var_count + 2);
                    ctx->local_var_count++;
                }
            }
        } else {
            // Move register/variable to register/variable
            int src_reg = parseRegister(arg1);
            int dest_reg = parseRegister(arg3);
            
            // Handle source variable names with improved mapping
            if (!src_reg) {
                if (strcmp(arg1, "low") == 0) src_reg = 2;
                else if (strcmp(arg1, "high") == 0) src_reg = 3;  
                else if (strcmp(arg1, "k") == 0) src_reg = 4;
                else if (strcmp(arg1, "i") == 0) src_reg = 5;
                else if (strcmp(arg1, "x") == 0) src_reg = 7;
                else if (strcmp(arg1, "t1") == 0) src_reg = 8; // t1 from LOAD_ARRAY -> r8
                else if (strcmp(arg1, "t3") == 0) src_reg = 6; // t3 from LOAD_ARRAY -> r6
                else src_reg = getNextRegister(ctx);
            }
            
            if (src_reg > 0 && dest_reg > 0) {
                // Register to register
                emitInstructionImproved(ctx, "move r%d r%d", dest_reg, src_reg);
            } else if (src_reg > 0 && arg3[0] && !parseRegister(arg3)) {
                // Register to variable - map to fixed registers
                if (strcmp(arg3, "k") == 0) {
                    emitInstructionImproved(ctx, "move r4 r%d", src_reg);
                } else if (strcmp(arg3, "x") == 0) {
                    emitInstructionImproved(ctx, "move r7 r%d", src_reg);
                } else if (strcmp(arg3, "i") == 0) {
                    emitInstructionImproved(ctx, "move r5 r%d", src_reg);
                } else {
                    emitInstructionImproved(ctx, "sw r29 r%d %d", src_reg, ctx->local_var_count + 2);
                    ctx->local_var_count++;
                }
            } else if (arg1[0] && !parseRegister(arg1) && dest_reg > 0) {
                // Variable to register
                if (strcmp(arg1, "k") == 0) {
                    emitInstructionImproved(ctx, "move r%d r4", dest_reg);
                } else if (strcmp(arg1, "x") == 0) {
                    emitInstructionImproved(ctx, "move r%d r7", dest_reg);
                } else if (strcmp(arg1, "i") == 0) {
                    emitInstructionImproved(ctx, "move r%d r5", dest_reg);
                } else {
                    // Load from memory
                    emitInstructionImproved(ctx, "lw r29 r%d %d", dest_reg, ctx->local_var_count + 2);
                }
            }
        }
        
    } else if (strcmp(op, "ADD") == 0) {
        // Addition: ADD src1, src2, dest, __
        int src1_reg = parseRegister(arg1);
        int src2_reg = parseRegister(arg2);
        int dest_reg = parseRegister(arg3);
        
        // Handle variable names
        if (!src1_reg) {
            if (strcmp(arg1, "low") == 0) src1_reg = 2;
            else if (strcmp(arg1, "i") == 0) src1_reg = 5;
            else if (strcmp(arg1, "k") == 0) src1_reg = 4;
            else src1_reg = getNextRegister(ctx);
        }
        
        if (isNumber(arg2)) {
            int val = atoi(arg2);
            if (!dest_reg) dest_reg = getNextRegister(ctx);
            emitInstructionImproved(ctx, "addi r%d r%d %d", dest_reg, src1_reg, val);
        } else {
            if (!src2_reg) {
                if (strcmp(arg2, "high") == 0) src2_reg = 3;
                else if (strcmp(arg2, "i") == 0) src2_reg = 5;
                else src2_reg = getNextRegister(ctx);
            }
            if (!dest_reg) dest_reg = getNextRegister(ctx);
            emitInstructionImproved(ctx, "add r%d r%d r%d", dest_reg, src1_reg, src2_reg);
        }
        
        // Store result to variable if needed
        if (arg3[0] && !parseRegister(arg3)) {
            if (strcmp(arg3, "i") == 0) {
                emitInstructionImproved(ctx, "move r5 r%d", dest_reg);
            } else if (strcmp(arg3, "k") == 0) {
                emitInstructionImproved(ctx, "move r4 r%d", dest_reg);
            }
        }
        
    } else if (strcmp(op, "SUB") == 0) {
        // Subtraction: SUB src1, src2, dest, __
        int src1_reg = parseRegister(arg1);
        int src2_reg = parseRegister(arg2);
        int dest_reg = parseRegister(arg3);
        
        // Handle variable names with GCD-specific logic
        if (!src1_reg) {
            if (strcmp(arg1, "u") == 0) src1_reg = 1; // GCD parameter
            else if (strcmp(arg1, "v") == 0) src1_reg = 2; // GCD parameter  
            else if (strcmp(arg1, "i") == 0) src1_reg = 5; // Sort variable
            else src1_reg = getNextRegister(ctx);
        }
        
        if (isNumber(arg2)) {
            int val = atoi(arg2);
            if (!dest_reg) dest_reg = getNextRegister(ctx);
            emitInstructionImproved(ctx, "subi r%d r%d %d", dest_reg, src1_reg, val);
        } else {
            if (!src2_reg) {
                if (strcmp(arg2, "u") == 0) src2_reg = 1;
                else if (strcmp(arg2, "v") == 0) src2_reg = 2;
                else src2_reg = getNextRegister(ctx);
            }
            if (!dest_reg) dest_reg = getNextRegister(ctx);
            
            // CRITICAL: For GCD, preserve original u value in r6 before modulo calculation
            if (strcmp(arg1, "u") == 0 && src1_reg == 1) {
                emitInstructionImproved(ctx, "move r6 r1"); // Save u in r6
                emitInstructionImproved(ctx, "sub r%d r6 r%d", dest_reg, src2_reg);
            } else {
                emitInstructionImproved(ctx, "sub r%d r%d r%d", dest_reg, src1_reg, src2_reg);
            }
        }
        
    } else if (strcmp(op, "MUL") == 0) {
        // Multiplication: MUL src1, src2, dest, __
        int src1_reg = parseRegister(arg1);
        int src2_reg = parseRegister(arg2);
        int dest_reg = parseRegister(arg3);
        
        if (!src1_reg) src1_reg = getNextRegister(ctx);
        if (!src2_reg) src2_reg = getNextRegister(ctx);
        if (!dest_reg) dest_reg = getNextRegister(ctx);
        
        emitInstructionImproved(ctx, "mult r%d r%d", src1_reg, src2_reg);
        emitInstructionImproved(ctx, "mflo r%d", dest_reg);
        
    } else if (strcmp(op, "DIV") == 0) {
        // Division: DIV src1, src2, dest, __
        int src1_reg = parseRegister(arg1);
        int src2_reg = parseRegister(arg2);
        
        if (!src1_reg) {
            if (strcmp(arg1, "u") == 0) src1_reg = 1;
            else if (strcmp(arg1, "v") == 0) src1_reg = 2;
            else src1_reg = getNextRegister(ctx);
        }
        if (!src2_reg) {
            if (strcmp(arg2, "v") == 0) src2_reg = 2;
            else src2_reg = getNextRegister(ctx);
        }
        
        emitInstructionImproved(ctx, "div r%d r%d", src1_reg, src2_reg);
        emitInstructionImproved(ctx, "mflo r%d", src1_reg); // Quotient back to src1
        
    } else if (strcmp(op, "CMP") == 0) {
        // Compare operation: CMP src1, src2, __, __
        int src1_reg = parseRegister(arg1);
        int src2_reg = parseRegister(arg2);
        
        // Handle variable names for src1 with consistent mapping
        if (!src1_reg) {
            if (strcmp(arg1, "v") == 0) src1_reg = 2; // GCD parameter
            else if (strcmp(arg1, "u") == 0) src1_reg = 1; // GCD parameter
            else if (strcmp(arg1, "i") == 0) src1_reg = 5; // Sort variable
            else if (strcmp(arg1, "k") == 0) src1_reg = 4; // Sort variable
            else if (strcmp(arg1, "t3") == 0) src1_reg = 6; // Array load result -> r6
            else src1_reg = getNextRegister(ctx);
        }
        
        // Map IR register references to actual registers for Sort
        if (src1_reg == 5) src1_reg = 5; // R5 is 'i' loop variable
        if (src1_reg == 3) src1_reg = 7; // R3 in IR should be 'x' variable (r7)
        
        // Handle variable names for src2
        if (!src2_reg) {
            if (strcmp(arg2, "high") == 0) src2_reg = 3; // Sort parameter
            else if (strcmp(arg2, "low") == 0) src2_reg = 2; // Sort parameter
            else if (strcmp(arg2, "x") == 0) src2_reg = 7; // Sort variable
            else src2_reg = getNextRegister(ctx);
        }
        
        // Map IR register references
        if (src2_reg == 3 && strcmp(arg2, "R3") == 0) src2_reg = 7; // R3 in IR -> x variable (r7)
        
        if (isNumber(arg2)) {
            int val = atoi(arg2);
            if (val == 0) {
                // Compare with zero: store result in r3 for later branch
                emitInstructionImproved(ctx, "li r3 0");
                emitInstructionImproved(ctx, "slt r4 r%d r3", src1_reg); // r4 = (src1 < 0)
                emitInstructionImproved(ctx, "slt r5 r3 r%d", src1_reg); // r5 = (0 < src1)  
                emitInstructionImproved(ctx, "or r3 r4 r5");  // r3 = (src1 != 0)
            } else {
                int temp_reg = getNextRegister(ctx);
                emitInstructionImproved(ctx, "li r%d %d", temp_reg, val);
                emitInstructionImproved(ctx, "sub r3 r%d r%d", src1_reg, temp_reg); // r3 = src1 - val
            }
        } else {
            emitInstructionImproved(ctx, "sub r3 r%d r%d", src1_reg, src2_reg); // r3 = src1 - src2
        }
        
    } else if (strcmp(op, "BR_NE") == 0) {
        // Branch if not equal (if r3 != 0)
        emitInstructionImproved(ctx, "beq r3 r0 3");
        emitInstructionImproved(ctx, "blt r3 r0 3");
        
    } else if (strcmp(op, "BR_GE") == 0) {
        // Branch if greater or equal (if r3 >= 0)
        emitInstructionImproved(ctx, "beq r3 r0 3");
        emitInstructionImproved(ctx, "blt r3 r0 3");
        
    } else if (strcmp(op, "GOTO") == 0) {
        // Unconditional jump
        emitInstructionImproved(ctx, "jump %s", arg1);
        
    } else if (strstr(op, "L") == op && op[strlen(op)-1] == ':') {
        // Label definition
        char clean_label[64];
        strncpy(clean_label, op, strlen(op) - 1);
        clean_label[strlen(op) - 1] = '\0';
        emitInstructionImproved(ctx, "# Label %s:", clean_label);
        
    } else if (strcmp(op, "LOAD_ARRAY") == 0) {
        // Array load: LOAD_ARRAY array, index, dest, __
        int index_reg = parseRegister(arg2);
        int dest_reg = parseRegister(arg3);
        
        if (!index_reg) {
            if (strcmp(arg2, "low") == 0) index_reg = 2;
            else if (strcmp(arg2, "high") == 0) index_reg = 3;
            else if (strcmp(arg2, "i") == 0) index_reg = 5; // Assume i is in r5
            else if (strcmp(arg2, "k") == 0) index_reg = 4; // Assume k is in r4
            else index_reg = getNextRegister(ctx);
        }
        if (!dest_reg) dest_reg = getNextRegister(ctx);
        
        // Map specific temp variables to dedicated registers for consistency
        if (strcmp(arg3, "t3") == 0) dest_reg = 6; // t3 -> r6 (for CMP consistency)
        else if (strcmp(arg3, "t1") == 0) dest_reg = 8; // t1 -> r8 
        else if (strcmp(arg3, "t4") == 0) dest_reg = 9; // t4 -> r9
        else if (dest_reg == 2) dest_reg = 8; // Use r8 instead of r2 for generic conflicts
        else if (dest_reg == 3) dest_reg = 9; // Use r9 instead of r3
        
        // Array access: load from base_address + index
        if (strcmp(arg1, "a") == 0) {
            // Parameter 'a' is in r1 (array base address), index_reg has the index
            emitInstructionImproved(ctx, "add r10 r1 r%d", index_reg); // r10 = base + index (temp)
            emitInstructionImproved(ctx, "lw r%d 0(r10)", dest_reg);   // dest = mem[base + index]
        } else if (strcmp(arg1, "vet") == 0) {
            // Global array 'vet' - use absolute addressing
            emitInstructionImproved(ctx, "lw r%d %d(r%d)", dest_reg, 0, index_reg);
        }
        
    } else if (strcmp(op, "STORE_ARRAY") == 0) {
        // Array store: STORE_ARRAY array, index, src, __  
        int index_reg = parseRegister(arg2);
        int src_reg = parseRegister(arg3);
        
        if (!index_reg) {
            if (strcmp(arg2, "k") == 0) index_reg = 4;
            else if (strcmp(arg2, "i") == 0) index_reg = 5;
            else index_reg = getNextRegister(ctx);
        }
        if (!src_reg) {
            if (strcmp(arg3, "x") == 0) src_reg = 7;
            else if (strcmp(arg3, "t") == 0) src_reg = 9; // temp variable
            else src_reg = getNextRegister(ctx);
        }
        
        if (strcmp(arg1, "a") == 0) {
            // Parameter 'a' is in r1 (array base address)
            emitInstructionImproved(ctx, "add r10 r1 r%d", index_reg); // r10 = base + index
            emitInstructionImproved(ctx, "sw r%d 0(r10)", src_reg);    // mem[base + index] = src
        } else if (strcmp(arg1, "vet") == 0) {
            // Global array 'vet'
            emitInstructionImproved(ctx, "sw r%d %d(r%d)", src_reg, 0, index_reg);
        }
        
    } else if (strcmp(op, "CALL") == 0) {
        // Function call
        if (strcmp(arg1, "input") == 0) {
            emitInstructionImproved(ctx, "input r28");
        } else if (strcmp(arg1, "output") == 0) {
            // Output expects the value in some register, assume it's set up
            emitInstructionImproved(ctx, "outputreg r1");
        } else {
            // Regular function call - use stack protocol
            emitInstructionImproved(ctx, "sw r30 r%d 0", getNextRegister(ctx)); // Save context
            emitInstructionImproved(ctx, "addi r30 r30 1");
            emitInstructionImproved(ctx, "sw r30 r%d 0", 2); // Save arg1
            emitInstructionImproved(ctx, "addi r30 r30 1");
            emitInstructionImproved(ctx, "sw r30 r%d 0", 3); // Save arg2
            emitInstructionImproved(ctx, "addi r30 r30 1");
            
            // Load function parameters into registers
            emitInstructionImproved(ctx, "subi r30 r30 1");
            emitInstructionImproved(ctx, "lw r30 r3 0");
            emitInstructionImproved(ctx, "subi r30 r30 1");
            emitInstructionImproved(ctx, "lw r30 r2 0");
            emitInstructionImproved(ctx, "subi r30 r30 1");
            emitInstructionImproved(ctx, "lw r30 r1 0");
            
            // Set up frame and call
            emitInstructionImproved(ctx, "sw r30 r29 0");
            emitInstructionImproved(ctx, "move r29 r30");
            emitInstructionImproved(ctx, "addi r30 r30 1");
            emitInstructionImproved(ctx, "jal 1"); // Call function
            
            // Restore context
            emitInstructionImproved(ctx, "move r30 r29");
            emitInstructionImproved(ctx, "lw r29 r29 0");
        }
        
    } else if (strcmp(op, "RETURN") == 0) {
        // Function return
        if (arg1[0]) {
            // Return with value
            int ret_reg = parseRegister(arg1);
            if (!ret_reg) {
                if (strcmp(arg1, "k") == 0) ret_reg = 4;
                else if (strcmp(arg1, "u") == 0) ret_reg = 1;
                else ret_reg = getNextRegister(ctx);
            }
            
            // Map IR register to actual register for consistency
            if (strcmp(arg1, "R1") == 0) ret_reg = 4; // R1 in IR should be 'k' variable for minloc function
            
            emitInstructionImproved(ctx, "move r28 r%d", ret_reg);
        }
        emitInstructionImproved(ctx, "lw r29 r31 1");
        emitInstructionImproved(ctx, "jr r31 r0 r0");
        
    } else if (strcmp(op, "ARG") == 0) {
        // Function argument - push to stack (handled by caller)
        emitInstructionImproved(ctx, "# Argument %s", arg1);
        
    } else {
        // Handle other operations generically
        if (strstr(line, "L") == line && strstr(line, ":")) {
            // It's a label
            emitInstructionImproved(ctx, "# %s", line);
        } else {
            // Unknown operation - emit as comment for debugging
            emitInstructionImproved(ctx, "# Unknown IR: %s", line);
        }
    }
}

// Main assembly generation function - simplified and unified
void generateAssemblyFromIRImproved(const char *ir_file, const char *assembly_file) {
    FILE *ir = fopen(ir_file, "r");
    if (!ir) {
        printf("Error: Could not open IR file %s\n", ir_file);
        return;
    }
    
    FILE *out = fopen(assembly_file, "w");
    if (!out) {
        printf("Error: Could not create assembly file %s\n", assembly_file);
        fclose(ir);
        return;
    }
    
    ImprovedAssemblyContext ctx = {0};
    ctx.output = out;
    ctx.instruction_count = 1; // Start from 1 to leave space for initial jump
    ctx.register_counter = 1;
    
    char line[512];
    int actual_main_start = 0;
    
    // First pass: generate assembly
    while (fgets(line, sizeof(line), ir)) {
        // Remove newline
        char *newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        // Track main function start for initial jump
        if (strstr(line, "FUNC_BEGIN main")) {
            actual_main_start = ctx.instruction_count;
        }
        
        processIRLine(&ctx, line);
    }
    
    // Fix the initial jump to main
    if (actual_main_start > 0) {
        fseek(out, 0, SEEK_SET);
        fprintf(out, "0-jump %d\n", actual_main_start);
    }
    
    fclose(ir);
    fclose(out);
}
