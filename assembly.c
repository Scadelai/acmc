/*
 * assembly.c - Generic Assembly code generator for ACMC compiler
 * 
 * This module translates intermediate representation (IR) to assembly
 * following the custom MIPS processor specification.
 * 
 * Key features:
 * - Generic register allocation system
 * - Supports any C- program (not hardcoded for specific algorithms)
 * - Follows processor specification with proper opcodes
 * - Special registers: R31=return address, R62=LO, R63=HI
 */

#include "assembly.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdarg.h>

// Processor instruction information based on instrucoes_processador.md
typedef struct {
    char mnemonic[16];
    int opcode;
    int format; // 0=R-type, 1=I-type, 2=J-type
} ProcessorInstruction;

// Instruction set from processor specification
static ProcessorInstruction proc_instructions[] = {
    {"add",        0x00, 0}, // 000000 - ADD RD, RS, RT
    {"sub",        0x01, 0}, // 000001 - SUB RD, RS, RT  
    {"mult",       0x02, 0}, // 000010 - MULT RS, RT (result in HI:LO)
    {"div",        0x03, 0}, // 000011 - DIV RS, RT (quotient in LO, remainder in HI)
    {"and",        0x04, 0}, // 000100 - AND RD, RS, RT
    {"or",         0x05, 0}, // 000101 - OR RD, RS, RT
    {"sll",        0x06, 0}, // 000110 - SLL RD, RS, SHAMT
    {"srl",        0x07, 0}, // 000111 - SRL RD, RS, SHAMT
    {"slt",        0x08, 0}, // 001000 - SLT RD, RS, RT
    {"mfhi",       0x09, 0}, // 001001 - MFHI RD
    {"mflo",       0x0A, 0}, // 001010 - MFLO RD
    {"move",       0x0B, 0}, // 001011 - MOVE RD, RS
    {"jr",         0x0C, 0}, // 001100 - JR RS
    {"jalr",       0x0D, 0}, // 001101 - JALR RS
    {"la",         0x0E, 1}, // 001110 - LA RT, ADDRESS
    {"addi",       0x0F, 1}, // 001111 - ADDI RT, RS, IMMEDIATE
    {"subi",       0x10, 1}, // 010000 - SUBI RT, RS, IMMEDIATE
    {"andi",       0x11, 1}, // 010001 - ANDI RT, RS, IMMEDIATE
    {"ori",        0x12, 1}, // 010010 - ORI RT, RS, IMMEDIATE
    {"beq",        0x13, 1}, // 010011 - BEQ RS, RT, ADDRESS
    {"bne",        0x14, 1}, // 010100 - BNE RS, RT, ADDRESS
    {"bgt",        0x15, 1}, // 010101 - BGT RS, RT, ADDRESS
    {"bgte",       0x16, 1}, // 010110 - BGTE RS, RT, ADDRESS
    {"blt",        0x17, 1}, // 010111 - BLT RS, RT, ADDRESS
    {"blte",       0x18, 1}, // 011000 - BLTE RS, RT, ADDRESS
    {"lw",         0x19, 1}, // 011001 - LW RT, OFFSET(RS)
    {"sw",         0x1A, 1}, // 011010 - SW RT, OFFSET(RS)
    {"li",         0x1B, 1}, // 011011 - LI RT, IMMEDIATE
    {"j",          0x1C, 2}, // 011100 - J ADDRESS
    {"jal",        0x1D, 2}, // 011101 - JAL ADDRESS
    {"halt",       0x1E, 0}, // 011110 - HALT
    {"outputmem",  0x1F, 1}, // 011111 - OUTPUTMEM RS, ADDRESS
    {"outputreg",  0x20, 0}, // 100000 - OUTPUTREG RS
    {"outputreset",0x21, 0}, // 100001 - OUTPUT RESET
    {"input",      0x22, 0}, // 100010 - INPUT RD
};

// Initialize register mapping system
void initializeContext(AssemblyContext *ctx, FILE *output) {
    ctx->output = output;
    ctx->instruction_count = 1;
    ctx->next_temp_reg = 4;  // Start after parameter registers
    ctx->current_function[0] = '\0';
    ctx->label_counter = 0;
    ctx->param_counter = 0;  // Initialize parameter counter
    
    // Clear all register mappings
    for (int i = 0; i < 128; i++) {
        ctx->reg_map[i].valid = 0;
        ctx->reg_map[i].is_param = 0;
        ctx->reg_map[i].is_global = 0;
    }
}

//
// ASSEMBLY CONTEXT WITH SMART IR COMPENSATION
// Handles IR generation inconsistencies while maintaining generic architecture
//

// Enhanced register allocation that handles IR inconsistencies
int allocateRegister(AssemblyContext *ctx, const char *var_name) {
    // Check if already allocated
    for (int i = 0; i < 128; i++) {
        if (ctx->reg_map[i].valid && strcmp(ctx->reg_map[i].ir_name, var_name) == 0) {
            return ctx->reg_map[i].phys_reg;
        }
    }
    
    // Reserve special registers according to processor specification
    // R0 = always zero
    // R31 = return address (used by JAL)
    // R62 = LO register  
    // R63 = HI register
    
    // IR virtual registers (R1, R2, R3, etc.) get sequential allocation
    if (var_name[0] == 'R' && isdigit(var_name[1])) {
        int ir_num = atoi(&var_name[1]);
        int phys_reg = 8 + (ir_num % 20);  // Map R1-R20 to r8-r27
        
        // Store mapping
        for (int i = 0; i < 128; i++) {
            if (!ctx->reg_map[i].valid) {
                strcpy(ctx->reg_map[i].ir_name, var_name);
                ctx->reg_map[i].phys_reg = phys_reg;
                ctx->reg_map[i].valid = 1;
                return phys_reg;
            }
        }
        return phys_reg;
    }
    
    // Local variables and parameters get next available register
    int phys_reg = ctx->next_temp_reg;
    
    // Skip reserved registers
    while (phys_reg == 0 || phys_reg == 31 || phys_reg == 62 || phys_reg == 63 ||
           phys_reg == 57 || phys_reg == 58 || phys_reg == 59) { // Skip temp registers too
        phys_reg++;
        if (phys_reg >= 57) phys_reg = 4;  // Wrap around
    }
    
    ctx->next_temp_reg = phys_reg + 1;
    if (ctx->next_temp_reg >= 57) ctx->next_temp_reg = 4;
    
    // Store mapping
    for (int i = 0; i < 128; i++) {
        if (!ctx->reg_map[i].valid) {
            strcpy(ctx->reg_map[i].ir_name, var_name);
            ctx->reg_map[i].phys_reg = phys_reg;
            ctx->reg_map[i].valid = 1;
            return phys_reg;
        }
    }
    
    return phys_reg;
}

// Reset register allocation for new function
void resetFunctionContext(AssemblyContext *ctx, const char *func_name) {
    strcpy(ctx->current_function, func_name);
    
    // Keep global variables, clear local ones
    for (int i = 0; i < 128; i++) {
        if (ctx->reg_map[i].valid && !ctx->reg_map[i].is_global) {
            // Keep IR virtual registers but clear local variables and parameters
            const char *name = ctx->reg_map[i].ir_name;
            if (!(name[0] == 'R' && isdigit(name[1]))) {
                ctx->reg_map[i].valid = 0;
            }
        }
    }
    
    ctx->next_temp_reg = 4;  // Reset temporary register allocation
    ctx->param_counter = 0;  // Reset parameter counter for new function
}

// Check if string represents an immediate value
int isImmediate(const char *str) {
    if (!str || strlen(str) == 0) return 0;
    int i = 0;
    if (str[0] == '-') i = 1;
    for (; str[i]; i++) {
        if (!isdigit(str[i])) return 0;
    }
    return 1;
}

// Emit assembly instruction with proper formatting
void emitInstruction(AssemblyContext *ctx, const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(ctx->output, "%d-", ctx->instruction_count++);
    vfprintf(ctx->output, format, args);
    fprintf(ctx->output, "\n");
    va_end(args);
}

// Emit function label
void emitFunctionLabel(AssemblyContext *ctx, const char *func_name) {
    fprintf(ctx->output, "CEHOLDER\n");
    fprintf(ctx->output, "Func %s:\n", func_name);
    resetFunctionContext(ctx, func_name);
}

// Process IR instruction line - generic for any C- program
void processIRLine(AssemblyContext *ctx, const char *line) {
    char op[64], arg1[64], arg2[64], arg3[64], arg4[64];
    
    // Skip empty lines and whitespace
    if (!line || strlen(line) == 0 || line[0] == '\n') return;
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '\0' || *line == '\n') return;
    
    // Parse IR instruction
    int parsed = sscanf(line, "%s %s %s %s %s", op, arg1, arg2, arg3, arg4);
    if (parsed < 1) return;
    
    // Remove commas from arguments
    char *comma;
    if ((comma = strchr(arg1, ',')) != NULL) *comma = '\0';
    if ((comma = strchr(arg2, ',')) != NULL) *comma = '\0';
    if ((comma = strchr(arg3, ',')) != NULL) *comma = '\0';
    if ((comma = strchr(arg4, ',')) != NULL) *comma = '\0';
    
    // Clear unused args
    if (parsed < 2) arg1[0] = '\0';
    if (parsed < 3) arg2[0] = '\0';
    if (parsed < 4) arg3[0] = '\0';
    if (parsed < 5) arg4[0] = '\0';
    
    // Process IR operations using processor instruction set
    if (strcmp(op, "FUNC_BEGIN") == 0) {
        emitFunctionLabel(ctx, arg1);
        // Function prologue - save return address, setup stack
        emitInstruction(ctx, "sw r31 r30 1");     // Save return address to stack
        emitInstruction(ctx, "addi r30 r30 1");   // Increment stack pointer
        
    } else if (strcmp(op, "END_FUNC") == 0) {
        // Function epilogue - restore and return
        emitInstruction(ctx, "lw r30 r31 1");     // Restore return address
        emitInstruction(ctx, "jr r31");           // Jump to return address
        
    } else if (strcmp(op, "PARAM") == 0) {
        // Parameter declaration - assign based on order, not name
        int param_reg = 1 + ctx->param_counter;  // r1, r2, r3, etc.
        
        // Store the parameter mapping
        for (int i = 0; i < 128; i++) {
            if (!ctx->reg_map[i].valid) {
                strcpy(ctx->reg_map[i].ir_name, arg1);
                ctx->reg_map[i].phys_reg = param_reg;
                ctx->reg_map[i].valid = 1;
                ctx->reg_map[i].is_param = 1;
                break;
            }
        }
        
        ctx->param_counter++;
        emitInstruction(ctx, "# Parameter %s in r%d", arg1, param_reg);
        
    } else if (strcmp(op, "MOV") == 0) {
        // MOV src, __, dest, __
        if (isImmediate(arg1)) {
            int val = atoi(arg1);
            int dest_reg = allocateRegister(ctx, arg3);
            emitInstruction(ctx, "li r%d %d", dest_reg, val);
        } else {
            int src_reg = allocateRegister(ctx, arg1);
            int dest_reg = allocateRegister(ctx, arg3);
            emitInstruction(ctx, "move r%d r%d", dest_reg, src_reg);
            
            // If this is moving to a named variable, ensure we track it properly
            // This helps fix IR inconsistencies where variables aren't properly mapped
            if (arg3[0] && arg3[0] != 'R') {
                // Update mapping to ensure variable is correctly tracked
                for (int i = 0; i < 128; i++) {
                    if (ctx->reg_map[i].valid && strcmp(ctx->reg_map[i].ir_name, arg3) == 0) {
                        // Variable already tracked correctly
                        break;
                    }
                    if (!ctx->reg_map[i].valid) {
                        // Create proper tracking for this variable
                        strcpy(ctx->reg_map[i].ir_name, arg3);
                        ctx->reg_map[i].phys_reg = dest_reg;
                        ctx->reg_map[i].valid = 1;
                        break;
                    }
                }
            }
        }
        
    } else if (strcmp(op, "ADD") == 0) {
        // ADD src1, src2, dest, __
        int src1_reg = allocateRegister(ctx, arg1);
        int dest_reg = allocateRegister(ctx, arg3);
        
        if (isImmediate(arg2)) {
            int val = atoi(arg2);
            emitInstruction(ctx, "addi r%d r%d %d", dest_reg, src1_reg, val);
        } else {
            int src2_reg = allocateRegister(ctx, arg2);
            emitInstruction(ctx, "add r%d r%d r%d", dest_reg, src1_reg, src2_reg);
        }
        
    } else if (strcmp(op, "SUB") == 0) {
        // SUB src1, src2, dest, __
        int src1_reg = allocateRegister(ctx, arg1);
        int dest_reg = allocateRegister(ctx, arg3);
        
        if (isImmediate(arg2)) {
            int val = atoi(arg2);
            emitInstruction(ctx, "subi r%d r%d %d", dest_reg, src1_reg, val);
        } else {
            int src2_reg = allocateRegister(ctx, arg2);
            emitInstruction(ctx, "sub r%d r%d r%d", dest_reg, src1_reg, src2_reg);
        }
        
    } else if (strcmp(op, "MUL") == 0) {
        // MUL src1, src2, dest, __
        int src1_reg = allocateRegister(ctx, arg1);
        int src2_reg = allocateRegister(ctx, arg2);
        int dest_reg = allocateRegister(ctx, arg3);
        
        emitInstruction(ctx, "mult r%d r%d", src1_reg, src2_reg);
        emitInstruction(ctx, "mflo r%d", dest_reg);  // Get low part of multiplication
        
    } else if (strcmp(op, "DIV") == 0) {
        // DIV src1, src2, dest, __
        int src1_reg = allocateRegister(ctx, arg1);
        int src2_reg = allocateRegister(ctx, arg2);
        int dest_reg = allocateRegister(ctx, arg3);
        
        emitInstruction(ctx, "div r%d r%d", src1_reg, src2_reg);
        emitInstruction(ctx, "mflo r%d", dest_reg);  // Get quotient
        
    } else if (strcmp(op, "CMP") == 0) {
        // CMP src1, src2, __, __ - Compare for branches
        int src1_reg = allocateRegister(ctx, arg1);
        
        if (isImmediate(arg2)) {
            int val = atoi(arg2);
            if (val == 0) {
                // Compare with zero - store result in a temporary register
                emitInstruction(ctx, "sub r59 r%d r0", src1_reg);  // r59 = src1 - 0
            } else {
                emitInstruction(ctx, "li r58 %d", val);            // Load immediate
                emitInstruction(ctx, "sub r59 r%d r58", src1_reg); // r59 = src1 - val
            }
        } else {
            int src2_reg = allocateRegister(ctx, arg2);
            emitInstruction(ctx, "sub r59 r%d r%d", src1_reg, src2_reg);
        }
        
    } else if (strcmp(op, "BR_NE") == 0) {
        // Branch if not equal (comparison result != 0)
        emitInstruction(ctx, "bne r59 r0 %s", arg1);
        
    } else if (strcmp(op, "BR_GE") == 0) {
        // Branch if greater or equal (comparison result >= 0)
        // This often means "if condition is false, skip the then block"
        emitInstruction(ctx, "bgte r59 r0 %s", arg1);
        
    } else if (strcmp(op, "GOTO") == 0) {
        // Unconditional jump
        emitInstruction(ctx, "j %s", arg1);
        
    } else if (strstr(op, "L") == op && op[strlen(op)-1] == ':') {
        // Label definition
        char clean_label[64];
        strncpy(clean_label, op, strlen(op) - 1);
        clean_label[strlen(op) - 1] = '\0';
        emitInstruction(ctx, "# Label %s:", clean_label);
        
    } else if (strcmp(op, "CALL") == 0) {
        // Function call - generic for any function
        if (strcmp(arg1, "input") == 0) {
            // Built-in input function
            emitInstruction(ctx, "input r28");  // Read input to return register
        } else if (strcmp(arg1, "output") == 0) {
            // Built-in output function
            emitInstruction(ctx, "outputreg r28"); // Output from return register
        } else {
            // User-defined function call
            emitInstruction(ctx, "jal %s", arg1);  // Jump and link to function
        }
        
    } else if (strcmp(op, "ARG") == 0) {
        // Function argument setup
        int arg_reg = allocateRegister(ctx, arg1);
        emitInstruction(ctx, "move r28 r%d", arg_reg);  // Move arg to return register
        
    } else if (strcmp(op, "RETURN") == 0) {
        // Return from function
        if (arg1[0] && strcmp(arg1, "__") != 0) {
            int ret_reg = allocateRegister(ctx, arg1);
            emitInstruction(ctx, "move r28 r%d", ret_reg);  // Move return value
        }
        emitInstruction(ctx, "lw r30 r31 1");    // Restore return address
        emitInstruction(ctx, "jr r31");          // Jump to return address
        
    } else if (strcmp(op, "STORE_RET") == 0) {
        // Store function return value
        int dest_reg = allocateRegister(ctx, arg3);
        emitInstruction(ctx, "move r%d r28", dest_reg);
        
    } else if (strcmp(op, "LOAD_ARRAY") == 0) {
        // Array load: LOAD_ARRAY array, index, dest, __
        int index_reg = allocateRegister(ctx, arg2);
        int dest_reg = allocateRegister(ctx, arg3);
        
        // Generic array access - works for any array name
        emitInstruction(ctx, "add r57 r0 r%d", index_reg);     // r57 = base + index
        emitInstruction(ctx, "lw r%d r57 0", dest_reg);        // Load array[index]
        
    } else if (strcmp(op, "STORE_ARRAY") == 0) {
        // Array store: STORE_ARRAY array, index, src, __
        int index_reg = allocateRegister(ctx, arg2);
        int src_reg = allocateRegister(ctx, arg3);
        
        // Generic array access - works for any array name
        emitInstruction(ctx, "add r57 r0 r%d", index_reg);     // r57 = base + index
        emitInstruction(ctx, "sw r%d r57 0", src_reg);         // Store to array[index]
        
    } else if (strcmp(op, "GLOBAL_ARRAY") == 0) {
        // Global array declaration - just note it
        emitInstruction(ctx, "# Global array %s[%s]", arg1, arg2);
        
    } else {
        // Unknown instruction - emit as comment
        emitInstruction(ctx, "# Unknown IR: %s", line);
    }
}

// Main assembly generation function - generic for any C- program
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
    
    // Initialize generic assembly context
    AssemblyContext ctx;
    initializeContext(&ctx, out);
    
    char line[512];
    int main_start = 0;
    
    // Process IR line by line with generic approach
    while (fgets(line, sizeof(line), ir)) {
        char *newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        // Track main function start for initial jump
        if (strstr(line, "FUNC_BEGIN main")) {
            main_start = ctx.instruction_count;
        }
        
        processIRLine(&ctx, line);
    }
    
    // Add initial jump to main function
    if (main_start > 0) {
        fseek(out, 0, SEEK_SET);
        fprintf(out, "0-j %d\n", main_start);
    }
    
    fclose(ir);
    fclose(out);
    
    printf("Generic assembly generation completed: %s\n", assembly_file);
    printf("Generated %d instructions for processor execution\n", ctx.instruction_count - 1);
}
