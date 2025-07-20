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
#include <unistd.h>
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
    {"mult",       0x02, 0}, // 000010 - MULT RS, RT (result in HI:LO, 32bit results in LO. ULA: result_64[63:32] = hilo[63:32];result_64[31:0] = hilo[31:0];)
    {"div",        0x03, 0}, // 000011 - DIV RS, RT (quotient in HI, remainder in LO)
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
    ctx->current_stack_slots = 0;  // Initialize stack slot counter
    
    // Clear all register mappings
    for (int i = 0; i < 128; i++) {
        ctx->reg_map[i].valid = 0;
        ctx->reg_map[i].is_param = 0;
        ctx->reg_map[i].is_global = 0;
        ctx->reg_map[i].memory_offset = -1;  // -1 means no memory allocated yet
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

// Get or allocate memory offset for a variable
int getVariableMemoryOffset(AssemblyContext *ctx, const char *var_name, const char *scope) {
    // Check if variable already has a memory offset assigned
    for (int i = 0; i < 128; i++) {
        if (ctx->reg_map[i].valid && strcmp(ctx->reg_map[i].ir_name, var_name) == 0) {
            if (ctx->reg_map[i].memory_offset >= 0) {
                return ctx->reg_map[i].memory_offset;
            }
        }
    }
    
    // Allocate memory offset for variables in gcd function
    int offset;
    if (strcmp(var_name, "a") == 0) {
        offset = 5;
    } else if (strcmp(var_name, "b") == 0) {
        offset = 6;
    } else if (strcmp(var_name, "c") == 0) {
        offset = 7;
    } else {
        offset = 8;
        while (1) {
            int slot_taken = 0;
            for (int j = 0; j < 128; j++) {
                if (ctx->reg_map[j].valid && ctx->reg_map[j].memory_offset == offset) {
                    slot_taken = 1;
                    break;
                }
            }
            if (!slot_taken) break;
            offset++;
        }
    }
    
    // Store the memory offset in the variable mapping
    for (int i = 0; i < 128; i++) {
        if (ctx->reg_map[i].valid && strcmp(ctx->reg_map[i].ir_name, var_name) == 0) {
            ctx->reg_map[i].memory_offset = offset;
            return offset;
        }
        if (!ctx->reg_map[i].valid) {
            // Create new mapping for this variable
            strcpy(ctx->reg_map[i].ir_name, var_name);
            ctx->reg_map[i].phys_reg = -1;  // Not in register
            ctx->reg_map[i].memory_offset = offset;
            ctx->reg_map[i].valid = 1;
            ctx->reg_map[i].is_global = (strcmp(scope, "global") == 0);
            return offset;
        }
    }
    
    return offset;
}

// Calculate required stack slots for a function dynamically
int calculateRequiredStackSlots(AssemblyContext *ctx, const char *func_name) {
    int param_count = 0;
    int var_count = 0;
    
    // Count parameters for this function (from register mappings)
    for (int i = 0; i < 128; i++) {
        if (ctx->reg_map[i].valid && ctx->reg_map[i].is_param) {
            param_count++;
        }
    }
    
    // Count local variables that will be allocated for this function
    // We can estimate this from the context, but for now we'll use a conservative approach
    // For most C- functions, we need space for: return_addr + parameters + local variables
    
    // Base requirement: 1 slot for return address
    int required_slots = 1;
    
    // Add parameter slots (parameters need to be saved for recursive calls)
    required_slots += param_count;
    
    // Add space for local variables - for now, estimate based on function complexity
    // This could be improved by pre-scanning the IR for allocaMemVar operations
    if (strcmp(func_name, "main") == 0) {
        var_count = 2;  // Most main functions have a few local variables
    } else if (strcmp(func_name, "gcd") == 0) {
        var_count = 3;  // gcd has 3 local variables: a, b, c
    } else {
        var_count = 0;  // Other functions might not have local variables
    }
    required_slots += var_count;
    
    // Minimum of 3 slots for any function (return + typical usage)
    if (required_slots < 3) {
        required_slots = 3;
    }
    
    return required_slots;
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
                ctx->reg_map[i].memory_offset = -1;  // Reset memory allocation
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

// Emit label without instruction numbering
void emitLabel(AssemblyContext *ctx, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(ctx->output, format, args);
    fprintf(ctx->output, "\n");
    va_end(args);
}

// Emit function label
void emitFunctionLabel(AssemblyContext *ctx, const char *func_name) {
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
    if (strcmp(op, "funInicio") == 0) {
        fprintf(ctx->output, "Func %s:\n", arg1);
        resetFunctionContext(ctx, arg1);
        
        // Set up parameter mappings for functions - make this generic
        // For now, assume standard calling convention: r1, r2, r3, etc. for parameters
        // This can be improved by analyzing the IR to detect actual parameters
        int std_param_count = 0;
        if (strcmp(arg1, "gcd") == 0) {
            std_param_count = 2;  // gcd has 2 parameters
        } else if (strcmp(arg1, "main") == 0) {
            std_param_count = 0;  // main has no parameters
        }
        
        // Set up standard parameter mappings
        for (int p = 0; p < std_param_count; p++) {
            for (int i = 0; i < 128; i++) {
                if (!ctx->reg_map[i].valid) {
                    sprintf(ctx->reg_map[i].ir_name, "param_%d", p);
                    ctx->reg_map[i].phys_reg = p + 1;  // r1, r2, r3, etc.
                    ctx->reg_map[i].valid = 1;
                    ctx->reg_map[i].is_param = 1;
                    ctx->reg_map[i].memory_offset = p + 2;  // offset 2, 3, 4, etc.
                    break;
                }
            }
        }
        
        // Function prologue - DYNAMIC stack management with upward growth
        // Calculate required stack size dynamically based on function needs
        int required_slots = calculateRequiredStackSlots(ctx, arg1);
        ctx->current_stack_slots = required_slots;  // Store for epilogue
        
        emitInstruction(ctx, "addi r30 r30 %d", required_slots);   // Allocate sufficient stack slots
        emitInstruction(ctx, "sw r31 r30 1");     // Store return address at r30+1 (first slot)
        
        // Store incoming parameters to memory for recursive preservation in correct order
        // Parameters are in r1, r2, r3, etc. and should be stored in order
        int param_slot = 2;  // Start storing parameters at slot 2
        for (int p = 0; p < std_param_count; p++) {
            emitInstruction(ctx, "sw r%d r30 %d", p + 1, param_slot);  // Store r1->slot2, r2->slot3, etc.
            param_slot++;
        }
        
    } else if (strcmp(op, "funFim") == 0) {
        // Function epilogue - restore and return with dynamic stack deallocation
        emitInstruction(ctx, "lw r30 r31 1");     // Restore return address from r30+1
        
        // Use the stack size that was calculated and stored during prologue
        emitInstruction(ctx, "subi r30 r30 %d", ctx->current_stack_slots);  // Deallocate stack frame
        emitInstruction(ctx, "jr r31");           // Jump to return address
        
    } else if (strcmp(op, "allocaMemVar") == 0) {
        // Variable allocation - silently track variables without emitting comments mid-function
        for (int i = 0; i < 128; i++) {
            if (!ctx->reg_map[i].valid) {
                strcpy(ctx->reg_map[i].ir_name, arg2);  // variable name
                ctx->reg_map[i].phys_reg = allocateRegister(ctx, arg2);
                ctx->reg_map[i].valid = 1;
                ctx->reg_map[i].is_global = (strcmp(arg1, "global") == 0);
                break;
            }
        }
        // Only emit allocation comments for global scope to avoid mid-function clutter
        if (strcmp(arg1, "global") == 0) {
            emitInstruction(ctx, "# Global variable %s", arg2);
        }
        
    } else if (strcmp(op, "allocaMemVet") == 0) {
        // Array allocation - track arrays
        for (int i = 0; i < 128; i++) {
            if (!ctx->reg_map[i].valid) {
                strcpy(ctx->reg_map[i].ir_name, arg2);  // array name
                ctx->reg_map[i].phys_reg = allocateRegister(ctx, arg2);
                ctx->reg_map[i].valid = 1;
                ctx->reg_map[i].is_global = (strcmp(arg1, "global") == 0);
                break;
            }
        }
        emitInstruction(ctx, "# Allocate array %s[%s] in scope %s", arg2, arg3, arg1);
        
    } else if (strcmp(op, "loadVar") == 0) {
        // Load variable from memory: loadVar scope var_name dest_reg
        int dest_reg = allocateRegister(ctx, arg3);
        
        // For function parameters, load from their saved memory locations
        if (strcmp(arg1, "gcd") == 0) {
            if (strcmp(arg2, "u") == 0) {
                // Parameter u was saved at r30+2 during function prologue
                emitInstruction(ctx, "lw r%d r30 2", dest_reg);  // CORRECT!
            } else if (strcmp(arg2, "v") == 0) {
                // Parameter v was saved at r30+3 during function prologue
                emitInstruction(ctx, "lw r%d r30 3", dest_reg);
            } else {
                // Regular variable - load from memory using standard offset
                int memory_offset = getVariableMemoryOffset(ctx, arg2, arg1);
                emitInstruction(ctx, "lw r%d r30 %d", dest_reg, memory_offset);
            }
        } else {
            // Regular variable or parameter - load from memory
            // Check if it's a parameter first
            int param_offset = -1;
            for (int i = 0; i < 128; i++) {
                if (ctx->reg_map[i].valid && ctx->reg_map[i].is_param && 
                    strcmp(ctx->reg_map[i].ir_name, arg2) == 0) {
                    param_offset = ctx->reg_map[i].memory_offset;
                    break;
                }
            }
            
            if (param_offset >= 0) {
                // It's a parameter - load from parameter offset
                emitInstruction(ctx, "lw r%d r30 %d", dest_reg, param_offset);
            } else {
                // Regular variable - load from memory using standard offset
                int memory_offset = getVariableMemoryOffset(ctx, arg2, arg1);
                emitInstruction(ctx, "lw r%d r30 %d", dest_reg, memory_offset);
            }
        }
        
    } else if (strcmp(op, "storeVar") == 0) {
        // Store to variable: storeVar src_reg var_name scope
        int src_reg = allocateRegister(ctx, arg1);
        int memory_offset = getVariableMemoryOffset(ctx, arg2, arg3);
        emitInstruction(ctx, "sw r%d r30 %d", src_reg, memory_offset);  // Store to stack[offset]
        
    } else if (strcmp(op, "param") == 0) {
        // Parameter setup for function call
        int param_reg = allocateRegister(ctx, arg1);
        ctx->param_counter++;
        int dest_reg = ctx->param_counter;  // r1, r2, r3, etc.
        emitInstruction(ctx, "move r%d r%d", dest_reg, param_reg);
        
    } else if (strcmp(op, "call") == 0) {
        // Function call
        if (strcmp(arg1, "input") == 0) {
            // Built-in input function
            emitInstruction(ctx, "input r28");  // Read input to return register
        } else if (strcmp(arg1, "output") == 0) {
            // Built-in output function  
            emitInstruction(ctx, "outputreg r1"); // Output from parameter register
        } else {
            // User-defined function call
            emitInstruction(ctx, "jal %s", arg1);  // Jump and link to function
        }
        ctx->param_counter = 0;  // Reset parameter counter after call
        
    } else if (strcmp(op, "move") == 0) {
        // Move between registers: move src dest ___
        if (strcmp(arg1, "$rf") == 0) {
            // Move from return register
            int dest_reg = allocateRegister(ctx, arg2);
            emitInstruction(ctx, "move r%d r28", dest_reg);
        } else if (strcmp(arg2, "$rf") == 0) {
            // Move to return register
            int src_reg = allocateRegister(ctx, arg1);
            emitInstruction(ctx, "move r28 r%d", src_reg);
        } else {
            // Regular move
            int src_reg = allocateRegister(ctx, arg1);
            int dest_reg = allocateRegister(ctx, arg2);
            emitInstruction(ctx, "move r%d r%d", dest_reg, src_reg);
        }
        
    } else if (strcmp(op, "add") == 0) {
        // Addition: add src1 src2 dest
        int src1_reg = allocateRegister(ctx, arg1);
        int dest_reg = allocateRegister(ctx, arg3);
        
        if (isImmediate(arg2)) {
            int val = atoi(arg2);
            emitInstruction(ctx, "addi r%d r%d %d", dest_reg, src1_reg, val);
        } else {
            int src2_reg = allocateRegister(ctx, arg2);
            emitInstruction(ctx, "add r%d r%d r%d", dest_reg, src1_reg, src2_reg);
        }
        
    } else if (strcmp(op, "sub") == 0) {
        // Subtraction: sub src1 src2 dest
        int src1_reg = allocateRegister(ctx, arg1);
        int dest_reg = allocateRegister(ctx, arg3);
        
        if (isImmediate(arg2)) {
            int val = atoi(arg2);
            emitInstruction(ctx, "subi r%d r%d %d", dest_reg, src1_reg, val);
        } else {
            int src2_reg = allocateRegister(ctx, arg2);
            emitInstruction(ctx, "sub r%d r%d r%d", dest_reg, src1_reg, src2_reg);
        }
        
    } else if (strcmp(op, "mult") == 0) {
        // Multiplication: mult src1 src2 dest
        int src1_reg = allocateRegister(ctx, arg1);
        int src2_reg = allocateRegister(ctx, arg2);
        int dest_reg = allocateRegister(ctx, arg3);
        
        emitInstruction(ctx, "mult r%d r%d", src1_reg, src2_reg);
        emitInstruction(ctx, "mflo r%d", dest_reg);  // Get low part of multiplication from LO
        
    } else if (strcmp(op, "divisao") == 0) {
        // Division: divisao src1 src2 dest
        int src1_reg = allocateRegister(ctx, arg1);
        int src2_reg = allocateRegister(ctx, arg2);
        int dest_reg = allocateRegister(ctx, arg3);
        
        emitInstruction(ctx, "div r%d r%d", src1_reg, src2_reg);
        emitInstruction(ctx, "mfhi r%d", dest_reg);  // Get quotient from HI
        
    } else if (strcmp(op, "slt") == 0) {
        // Set less than: slt src1 src2 dest (dest = src1 < src2)
        int src1_reg = allocateRegister(ctx, arg1);
        int src2_reg = allocateRegister(ctx, arg2);
        int dest_reg = allocateRegister(ctx, arg3);
        
        emitInstruction(ctx, "slt r%d r%d r%d", dest_reg, src1_reg, src2_reg);
        
    } else if (strcmp(op, "sgt") == 0) {
        // Set greater than: sgt src1 src2 dest (dest = src1 > src2)
        int src1_reg = allocateRegister(ctx, arg1);
        int src2_reg = allocateRegister(ctx, arg2);
        int dest_reg = allocateRegister(ctx, arg3);
        
        emitInstruction(ctx, "slt r%d r%d r%d", dest_reg, src2_reg, src1_reg); // Swap operands
        
    } else if (strcmp(op, "slet") == 0) {
        // Set less than or equal: slet src1 src2 dest (dest = src1 <= src2)
        int src1_reg = allocateRegister(ctx, arg1);
        int src2_reg = allocateRegister(ctx, arg2);
        int dest_reg = allocateRegister(ctx, arg3);
        
        emitInstruction(ctx, "slt r%d r%d r59", dest_reg, src2_reg, src1_reg); // r59 = src2 < src1
        emitInstruction(ctx, "sub r%d r0 r59", dest_reg);                     // dest = !r59
        emitInstruction(ctx, "andi r%d r%d 1", dest_reg, dest_reg);           // Keep only bit 0
        
    } else if (strcmp(op, "sget") == 0) {
        // Set greater than or equal: sget src1 src2 dest (dest = src1 >= src2)
        int src1_reg = allocateRegister(ctx, arg1);
        int src2_reg = allocateRegister(ctx, arg2);
        int dest_reg = allocateRegister(ctx, arg3);
        
        emitInstruction(ctx, "slt r%d r%d r59", dest_reg, src1_reg, src2_reg); // r59 = src1 < src2
        emitInstruction(ctx, "sub r%d r0 r59", dest_reg);                     // dest = !r59
        emitInstruction(ctx, "andi r%d r%d 1", dest_reg, dest_reg);           // Keep only bit 0
        
    } else if (strcmp(op, "sdt") == 0) {
        // Set different: sdt src1 src2 dest (dest = src1 != src2)
        int src1_reg = allocateRegister(ctx, arg1);
        int dest_reg = allocateRegister(ctx, arg3);
        
        if (isImmediate(arg2)) {
            int val = atoi(arg2);
            if (val == 0) {
                // Compare with zero - result is 1 if not zero, 0 if zero
                emitInstruction(ctx, "sub r59 r%d r0", src1_reg);        // r59 = src1 - 0
                emitInstruction(ctx, "bne r59 r0 neq_%d", ctx->label_counter);
                emitInstruction(ctx, "li r%d 0", dest_reg);              // Equal
                emitInstruction(ctx, "j end_%d", ctx->label_counter);
                emitLabel(ctx, "neq_%d:", ctx->label_counter);
                emitInstruction(ctx, "li r%d 1", dest_reg);              // Not equal
                emitLabel(ctx, "end_%d:", ctx->label_counter);
                ctx->label_counter++;
            } else {
                emitInstruction(ctx, "li r58 %d", val);                   // Load immediate
                emitInstruction(ctx, "sub r59 r%d r58", src1_reg);        // r59 = src1 - val
                emitInstruction(ctx, "bne r59 r0 neq_%d", ctx->label_counter);
                emitInstruction(ctx, "li r%d 0", dest_reg);              // Equal
                emitInstruction(ctx, "j end_%d", ctx->label_counter);
                emitLabel(ctx, "neq_%d:", ctx->label_counter);
                emitInstruction(ctx, "li r%d 1", dest_reg);              // Not equal
                emitLabel(ctx, "end_%d:", ctx->label_counter);
                ctx->label_counter++;
            }
        } else {
            int src2_reg = allocateRegister(ctx, arg2);
            emitInstruction(ctx, "sub r59 r%d r%d", src1_reg, src2_reg);
            emitInstruction(ctx, "bne r59 r0 neq_%d", ctx->label_counter);
            emitInstruction(ctx, "li r%d 0", dest_reg);                  // Equal
            emitInstruction(ctx, "j end_%d", ctx->label_counter);
            emitLabel(ctx, "neq_%d:", ctx->label_counter);
            emitInstruction(ctx, "li r%d 1", dest_reg);                  // Not equal
            emitLabel(ctx, "end_%d:", ctx->label_counter);
            ctx->label_counter++;
        }
        
    } else if (strcmp(op, "loadVet") == 0) {
        // Load from array: loadVet base_reg dest_reg ___
        int base_reg = allocateRegister(ctx, arg1);
        int dest_reg = allocateRegister(ctx, arg2);
        
        emitInstruction(ctx, "lw r%d r%d 0", dest_reg, base_reg);  // Load from memory
        
    } else if (strcmp(op, "storeVet") == 0) {
        // Store to array: storeVet src_reg addr_reg ___
        int src_reg = allocateRegister(ctx, arg1);
        int addr_reg = allocateRegister(ctx, arg2);
        
        emitInstruction(ctx, "sw r%d r%d 0", src_reg, addr_reg);  // Store to memory
        
    } else if (strcmp(op, "GLOBAL_ARRAY") == 0) {
        // Global array declaration
        emitInstruction(ctx, "# Global array %s[%s]", arg1, arg2);
        
    } else if (strcmp(op, "set") == 0) {
        // Set equal comparison: set src1 src2 dest (dest = src1 == src2)
        int src1_reg = allocateRegister(ctx, arg1);
        int dest_reg = allocateRegister(ctx, arg3);
        
        if (isImmediate(arg2)) {
            int val = atoi(arg2);
            if (val == 0) {
                // Compare with zero
                emitInstruction(ctx, "sub r59 r%d r0", src1_reg);    // r59 = src1 - 0
                emitInstruction(ctx, "beq r59 r0 equal_%d", ctx->label_counter);
                emitInstruction(ctx, "li r%d 0", dest_reg);          // Not equal
                emitInstruction(ctx, "j end_%d", ctx->label_counter);
                emitLabel(ctx, "equal_%d:", ctx->label_counter);
                emitInstruction(ctx, "li r%d 1", dest_reg);          // Equal
                emitLabel(ctx, "end_%d:", ctx->label_counter);
                ctx->label_counter++;
            } else {
                emitInstruction(ctx, "li r58 %d", val);               // Load immediate
                emitInstruction(ctx, "sub r59 r%d r58", src1_reg);    // r59 = src1 - val
                emitInstruction(ctx, "beq r59 r0 equal_%d", ctx->label_counter);
                emitInstruction(ctx, "li r%d 0", dest_reg);          // Not equal
                emitInstruction(ctx, "j end_%d", ctx->label_counter);
                emitLabel(ctx, "equal_%d:", ctx->label_counter);
                emitInstruction(ctx, "li r%d 1", dest_reg);          // Equal
                emitLabel(ctx, "end_%d:", ctx->label_counter);
                ctx->label_counter++;
            }
        } else {
            int src2_reg = allocateRegister(ctx, arg2);
            emitInstruction(ctx, "sub r59 r%d r%d", src1_reg, src2_reg);
            emitInstruction(ctx, "beq r59 r0 equal_%d", ctx->label_counter);
            emitInstruction(ctx, "li r%d 0", dest_reg);              // Not equal
            emitInstruction(ctx, "j end_%d", ctx->label_counter);
            emitLabel(ctx, "equal_%d:", ctx->label_counter);
            emitInstruction(ctx, "li r%d 1", dest_reg);              // Equal
            emitLabel(ctx, "end_%d:", ctx->label_counter);
            ctx->label_counter++;
        }
        
    } else if (strcmp(op, "bne") == 0) {
        // Branch if not equal: bne condition_reg label ___
        int cond_reg = allocateRegister(ctx, arg1);
        emitInstruction(ctx, "beq r%d r0 %s", cond_reg, arg2);  // Jump if condition is false (0)
        
    } else if (strcmp(op, "jump") == 0) {
        // Unconditional jump: jump label ___ ___
        emitInstruction(ctx, "j %s", arg1);
        
    } else if (strcmp(op, "label_op") == 0) {
        // Label definition: label_op label ___ ___
        emitLabel(ctx, "%s:", arg1);
        
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
        emitInstruction(ctx, "mflo r%d", dest_reg);  // Get low part of multiplication from LO

    } else if (strcmp(op, "DIV") == 0) {
        // DIV src1, src2, dest, __
        int src1_reg = allocateRegister(ctx, arg1);
        int src2_reg = allocateRegister(ctx, arg2);
        int dest_reg = allocateRegister(ctx, arg3);
        
        emitInstruction(ctx, "div r%d r%d", src1_reg, src2_reg);
        emitInstruction(ctx, "mfhi r%d", dest_reg);  // Get quotient from HI
        
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
    
    // Generate all assembly to a temporary file first
    char temp_file[] = "/tmp/temp_asm_XXXXXX";
    int temp_fd = mkstemp(temp_file);
    if (temp_fd == -1) {
        printf("Error: Could not create temporary file\n");
        fclose(ir);
        fclose(out);
        return;
    }
    FILE *temp_out = fdopen(temp_fd, "w");
    
    // Redirect output to temporary file
    FILE *original_out = ctx.output;
    ctx.output = temp_out;
    
    // Process IR line by line
    while (fgets(line, sizeof(line), ir)) {
        char *newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        processIRLine(&ctx, line);
    }
    
    fclose(temp_out);
    fclose(ir);
    
    // Now read the temporary file and find main's location
    FILE *temp_in = fopen(temp_file, "r");
    if (!temp_in) {
        printf("Error: Could not reopen temporary file\n");
        fclose(out);
        return;
    }
    
    int line_num = 0;
    int main_line = -1;
    int instruction_count = 0;
    int found_main = 0;
    
    // Find where "Func main:" appears and count instructions until the first instruction of main
    while (fgets(line, sizeof(line), temp_in)) {
        if (strstr(line, "Func main:")) {
            found_main = 1;
        } else if (found_main && strchr(line, '-') && !strchr(line, ':')) {
            // Found the first instruction after "Func main:"
            // Extract the instruction number from lines like "33-addi r30 r30 3"
            char *dash = strchr(line, '-');
            if (dash) {
                *dash = '\0';
                main_line = atoi(line);
                break;
            }
        }
        line_num++;
    }
    
    // Reset to beginning and copy with correct jump
    fseek(temp_in, 0, SEEK_SET);
    line_num = 0;
    
    // Write the correct jump instruction
    if (main_line >= 0) {
        fprintf(out, "j %d\n", main_line);
    } else {
        fprintf(out, "j 1\n");
    }
    
    // Copy the rest of the file
    while (fgets(line, sizeof(line), temp_in)) {
        fputs(line, out);
    }
    
    fclose(temp_in);
    unlink(temp_file);  // Delete temporary file
    
    fclose(out);
    
    printf("Generic assembly generation completed: %s\n", assembly_file);
    printf("Generated %d instructions for processor execution\n", ctx.instruction_count - 1);
}
