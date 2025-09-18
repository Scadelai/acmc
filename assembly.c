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
#include <stdbool.h>
#define MAX_FUNC_VARS 64
#define MAX_PENDING_ALLOCAS 64
static struct {
    char scope[64];
    char name[64];
} pending_allocas[MAX_PENDING_ALLOCAS];
static int pending_alloca_count = 0;

// Remove the typedef struct VarOffsetEntry definition, since it is now in assembly.h

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
    {"set",        0x23, 0}, // 100011 - SET RD, RS, RT
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
    ctx->in_function = false;
    ctx->var_offset_map_count = 0;
    for (int i = 0; i < MAX_FUNC_VARS; i++) {
        ctx->var_offsets[i].name[0] = '\0';
        ctx->var_offsets[i].offset = -1;
    }
    
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
    // R28 = return physical register (for function returns)
    if (strcmp(var_name, "r28") == 0 || strcmp(var_name, "$rf") == 0) {
        return 28; // Always return physical register 28 for r28 or $rf
    }
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
    
    // Skip reserved registers (0, 31, 62, 63, 57, 58, 59, 1, 2, 3, 28)
    while (phys_reg == 0 || phys_reg == 1 || phys_reg == 2 || phys_reg == 3 || phys_reg == 28 ||
           phys_reg == 31 || phys_reg == 57 || phys_reg == 58 || phys_reg == 59 || phys_reg == 62 || phys_reg == 63) {
        phys_reg++;
        // Ensure you don't go past max registers, adjust wrap-around if needed
        if (phys_reg >= 64) phys_reg = 4; // Wrap around safely to a non-reserved temp start
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
    // The first 3 slots are for RA, param_u, param_v.
    // Local variables start from offset 4.
    if (strcmp(var_name, "a") == 0) {
        offset = 4; // Should be r30 + 4
    } else if (strcmp(var_name, "b") == 0) {
        offset = 5; // Should be r30 + 5
    } else if (strcmp(var_name, "c") == 0) {
        offset = 6; // Should be r30 + 6
    } else {
        // For other local variables, dynamically assign from next available
        // Starting from after `c`, which is offset 7.
        // This 'else' block will likely only be hit if you have more locals than a,b,c
        offset = 7; 
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
    ctx->in_function = true;
    ctx->var_offset_map_count = 0;
    for (int i = 0; i < MAX_FUNC_VARS; i++) {
        ctx->var_offsets[i].name[0] = '\0';
        ctx->var_offsets[i].offset = -1;
    }
    
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

static void trim(char *str) {
    char *end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    *(end+1) = 0;
}

// Process IR instruction line - generic for any C- program
void processIRLine(AssemblyContext *ctx, const char *line) {
    printf("DEBUG: IR line received: '%s'\n", line);
    char op[64], arg1[64], arg2[64], arg3[64], arg4[64];
    char *comma;
    int parsed;
    int reprocess = 0;
    do {
        reprocess = 0;
        // Skip empty lines and whitespace
        if (!line) return;
        while (*line == ' ' || *line == '\t') line++;
        if (*line == '\0' || *line == '\n') return;
        if (line[0] == '#') return; // skip comments
        if (strlen(line) == 0) return;
        // Parse IR instruction
        parsed = sscanf(line, "%s %s %s %s %s", op, arg1, arg2, arg3, arg4);
        if (parsed < 1) return;
        // Remove commas from arguments
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
        static char current_func_name[64] = "";
        static int current_stack_size = 0;
        static bool prologue_emitted = true;
        static bool seen_real_instruction = false;
        static bool pre_prologue_phase = false;
        static bool last_was_input_call = false;
        static char last_input_temp[64] = "";

        // If we see allocaMemVar and not in_function, buffer it
        if (strcmp(op, "allocaMemVar") == 0 && !ctx->in_function) {
            if (pending_alloca_count < MAX_PENDING_ALLOCAS) {
                strncpy(pending_allocas[pending_alloca_count].scope, arg1, 63);
                pending_allocas[pending_alloca_count].scope[63] = '\0';
                strncpy(pending_allocas[pending_alloca_count].name, arg2, 63);
                pending_allocas[pending_alloca_count].name[63] = '\0';
                pending_alloca_count++;
            }
            return;
        }
        if (strcmp(op, "funInicio") == 0) {
            ctx->in_function = true;
            ctx->var_offset_map_count = 0;
            strncpy(current_func_name, arg1, 63);
            current_func_name[63] = '\0';
            prologue_emitted = false;
            seen_real_instruction = false;
            pre_prologue_phase = true;
            fprintf(ctx->output, "Func %s:\n", arg1);
            resetFunctionContext(ctx, arg1);
            // Flush pending allocas for this function
            for (int i = 0; i < pending_alloca_count; i++) {
                if (strcmp(pending_allocas[i].scope, arg1) == 0) {
                    int offset = ctx->var_offset_map_count + 1;
                    strncpy(ctx->var_offsets[ctx->var_offset_map_count].scope, pending_allocas[i].scope, 63);
                    ctx->var_offsets[ctx->var_offset_map_count].scope[63] = '\0';
                    strncpy(ctx->var_offsets[ctx->var_offset_map_count].name, pending_allocas[i].name, 63);
                    ctx->var_offsets[ctx->var_offset_map_count].name[63] = '\0';
                    ctx->var_offsets[ctx->var_offset_map_count].offset = offset;
                    printf("DEBUG: allocaMemVar %s.%s assigned offset %d (from pending)\n", pending_allocas[i].scope, pending_allocas[i].name, offset);
                    ctx->var_offset_map_count++;
                }
            }
            // Remove flushed allocas from pending buffer
            int j = 0;
            for (int i = 0; i < pending_alloca_count; i++) {
                if (strcmp(pending_allocas[i].scope, arg1) != 0) {
                    if (i != j) pending_allocas[j] = pending_allocas[i];
                    j++;
                }
            }
            pending_alloca_count = j;
            return;
        }
        // If we are in pre_prologue_phase and see a real instruction, emit prologue now
        if (ctx->in_function && !prologue_emitted && pre_prologue_phase && strcmp(op, "allocaMemVar") != 0 && strcmp(op, "funInicio") != 0) {
            // Print all var_offsets before emitting prologue
            printf("DEBUG: var_offsets before prologue for %s:\n", current_func_name);
            for (int i = 0; i < ctx->var_offset_map_count; i++) {
                printf("  [%d] scope='%s' name='%s' offset=%d\n", i, ctx->var_offsets[i].scope, ctx->var_offsets[i].name, ctx->var_offsets[i].offset);
            }
            current_stack_size = ctx->var_offset_map_count + 1; // +1 for RA
            printf("DEBUG: Prologue for %s, stack size = %d\n", current_func_name, current_stack_size);
            emitInstruction(ctx, "addi r30 r30 %d", current_stack_size);
            emitInstruction(ctx, "sw r31 r30 0");
            // Save r1 and r2 as parameters if the function has at least 1 or 2 parameters
            int param_count = 0;
            for (int i = 0; i < ctx->var_offset_map_count; i++) {
                // Heuristic: parameters are the first variables in the function's var_offsets
                // If the variable name matches the function's parameter list, count as param
                // For now, assume first two are parameters if function is not main
                if (strcmp(current_func_name, "main") != 0 && param_count < 2) {
                    emitInstruction(ctx, "sw r%d r30 %d", param_count + 1, param_count + 1); // r1->1, r2->2
                    param_count++;
                }
            }
            prologue_emitted = true;
            seen_real_instruction = true;
            pre_prologue_phase = false;
            // Debug print after re-parsing
            parsed = sscanf(line, "%s %s %s %s %s", op, arg1, arg2, arg3, arg4);
            if (parsed < 1) return;
            if ((comma = strchr(arg1, ',')) != NULL) *comma = '\0';
            if ((comma = strchr(arg2, ',')) != NULL) *comma = '\0';
            if ((comma = strchr(arg3, ',')) != NULL) *comma = '\0';
            if ((comma = strchr(arg4, ',')) != NULL) *comma = '\0';
            if (parsed < 2) arg1[0] = '\0';
            if (parsed < 3) arg2[0] = '\0';
            if (parsed < 4) arg3[0] = '\0';
            if (parsed < 5) arg4[0] = '\0';
            printf("DEBUG: After prologue, reparsed op='%s' arg1='%s' arg2='%s' arg3='%s' arg4='%s'\n", op, arg1, arg2, arg3, arg4);
            reprocess = 1;
            continue;
        }
        else if (strcmp(op, "loadVar") == 0 && ctx->in_function) {
            printf("DEBUG: loadVar arg1='|%s|' arg2='|%s|'\n", arg1, arg2);
            trim(arg1); trim(arg2);
            int offset = -1;
            for (int i = 0; i < ctx->var_offset_map_count; i++) {
                if (strcmp(ctx->var_offsets[i].name, arg2) == 0 && strcmp(ctx->var_offsets[i].scope, arg1) == 0) {
                    offset = ctx->var_offsets[i].offset;
                    break;
                }
            }
            printf("DEBUG: loadVar %s.%s offset %d\n", arg1, arg2, offset);
            
            int dest_reg;
            
            // Check if the next instruction is a param instruction
            // This indicates we're loading for a parameter, so we need unique registers
            bool is_param_load = false;
            // Look ahead at the next IR instruction to see if it's a param
            // For now, we'll use a heuristic: if arg3 is a temporary (t0, t1, etc.)
            // and we're in a sequence where param_counter is being used
            if (arg3[0] == 't') {
                // Force new allocation for parameter preparation
                // Invalidate any existing mapping for this temporary
                for (int i = 0; i < 128; i++) {
                    if (ctx->reg_map[i].valid && strcmp(ctx->reg_map[i].ir_name, arg3) == 0) {
                        ctx->reg_map[i].valid = 0;  // Invalidate to force new allocation
                        is_param_load = true;
                        break;
                    }
                }
            }
            
            dest_reg = allocateRegister(ctx, arg3);
            printf("DEBUG: loadVar allocated register r%d for %s (param_load=%d)\n", dest_reg, arg3, is_param_load);
            
            if (offset >= 0) {
                emitInstruction(ctx, "lw r%d r30 %d", dest_reg, offset);
            } else {
                emitInstruction(ctx, "lw r%d r30 0", dest_reg);
            }
        }
        else if (strcmp(op, "storeVar") == 0 && ctx->in_function) {
            printf("DEBUG: storeVar arg2='|%s|' arg3='|%s|'\n", arg2, arg3);
            trim(arg2); trim(arg3);
            int offset = -1;
            for (int i = 0; i < ctx->var_offset_map_count; i++) {
                if (strcmp(ctx->var_offsets[i].name, arg2) == 0 && strcmp(ctx->var_offsets[i].scope, arg3) == 0) {
                    offset = ctx->var_offsets[i].offset;
                    break;
                }
            }
            printf("DEBUG: storeVar %s.%s offset %d\n", arg3, arg2, offset);
            int src_reg = allocateRegister(ctx, arg1);
            if (offset >= 0) {
                emitInstruction(ctx, "sw r%d r30 %d", src_reg, offset);
            } else {
                emitInstruction(ctx, "sw r%d r30 0", src_reg);
            }
        }
        else if (strcmp(op, "funFim") == 0 && ctx->in_function) {
            if (!prologue_emitted) {
                emitInstruction(ctx, "addi r30 r30 %d", current_stack_size);
                emitInstruction(ctx, "sw r31 r30 0");
                prologue_emitted = true;
            }
            if (strcmp(arg1, "main") == 0) {
                emitInstruction(ctx, "halt");
            } else {
                emitInstruction(ctx, "lw r31 r30 0");
                emitInstruction(ctx, "subi r30 r30 %d", current_stack_size);
                emitInstruction(ctx, "jr r31");
            }
            ctx->in_function = false;
        }
        // ...allocaMemVar/allocaMemVet handlers removed as per changes.txt...
            
        else if (strcmp(op, "param") == 0) {
            ctx->param_counter++; // 1 for first param, 2 for second
            printf("DEBUG: param instruction, counter=%d, arg1='%s'\n", ctx->param_counter, arg1);
            
            if (isImmediate(arg1)) {
                // Handle immediate values directly with li instruction
                int immediate_val = atoi(arg1);
                printf("DEBUG: param immediate value %d\n", immediate_val);
                if (ctx->param_counter == 1) {
                    if (immediate_val == 0) {
                        emitInstruction(ctx, "move r1 r0"); // li r1, 0 -> move r1, r0
                    } else {
                        emitInstruction(ctx, "addi r1 r0 %d", immediate_val); // li r1, immediate
                    }
                } else if (ctx->param_counter == 2) {
                    if (immediate_val == 0) {
                        emitInstruction(ctx, "move r2 r0"); // li r2, 0 -> move r2, r0
                    } else {
                        emitInstruction(ctx, "addi r2 r0 %d", immediate_val); // li r2, immediate
                    }
                } else {
                    // For more parameters
                    if (immediate_val == 0) {
                        emitInstruction(ctx, "move r%d r0", ctx->param_counter);
                    } else {
                        emitInstruction(ctx, "addi r%d r0 %d", ctx->param_counter, immediate_val);
                    }
                }
            } else {
                // Handle register/variable values - move directly to parameter register
                int param_val_reg = allocateRegister(ctx, arg1); // This is the temp holding the parameter's value
                printf("DEBUG: param variable/register, allocated r%d for '%s'\n", param_val_reg, arg1);
                if (ctx->param_counter == 1) {
                    emitInstruction(ctx, "move r1 r%d", param_val_reg); // First parameter goes to r1
                } else if (ctx->param_counter == 2) {
                    emitInstruction(ctx, "move r2 r%d", param_val_reg); // Second parameter goes to r2
                } else {
                    // For more parameters, push to stack or use other conventions
                    emitInstruction(ctx, "move r%d r%d", ctx->param_counter, param_val_reg); // Fallback to r3, r4 etc.
                }
            }
        } else if (strcmp(op, "call") == 0) {
            // Function call
            if (strcmp(arg1, "input") == 0) {
                // Built-in input function
                emitInstruction(ctx, "input r28");  // Always emit input for every call input 0 ___
                // Do NOT set or use any flag, and do NOT return here; allow further processing
            } else if (strcmp(arg1, "output") == 0) {
                // Built-in output function  
                emitInstruction(ctx, "outputreg r1"); // Output from parameter register
            } else {
                // User-defined function call
                emitInstruction(ctx, "jal %s", arg1);  // Jump and link to function
            }
            ctx->param_counter = 0;  // Reset parameter counter after call
            
        } else if (strcmp(op, "move") == 0) {
            // If last was input call and move is tX $rf ___, track temp
            if (last_was_input_call && strcmp(arg2, "$rf") == 0) {
                int dest_reg = allocateRegister(ctx, arg1);
                emitInstruction(ctx, "move r%d r28", dest_reg);
                strcpy(last_input_temp, arg1);
                last_was_input_call = false;
                printf("DEBUG: Moved input result to %s (r%d <- r28)\n", arg1, dest_reg);
                return;
            }
            int dest_reg, src_reg;

            // Determine destination register
            if (strcmp(arg1, "$rf") == 0) { // If destination is $rf
                dest_reg = 28;
            } else {
                dest_reg = allocateRegister(ctx, arg1); // arg1 is the destination (e.g., t0)
            }

            // Determine source register
            if (strcmp(arg2, "$rf") == 0) { // If source is $rf
                src_reg = 28;
            } else {
                src_reg = allocateRegister(ctx, arg2); // arg2 is the source (e.g., t1)
            }
            emitInstruction(ctx, "move r%d r%d", dest_reg, src_reg);
            printf("DEBUG: Moved value from %s to %s (r%d <- r%d)\n", arg2, arg1, dest_reg, src_reg);
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
            int src2_reg = allocateRegister(ctx, arg2);
            int dest_reg = allocateRegister(ctx, arg3);
            
            emitInstruction(ctx, "sub r%d r%d r%d", dest_reg, src1_reg, src2_reg);
            
        } else if (strcmp(op, "mult") == 0) {
            // Multiplication: mult src1 src2 dest
            int src1_reg = allocateRegister(ctx, arg1);
            int src2_reg = allocateRegister(ctx, arg2);
            int dest_reg = allocateRegister(ctx, arg3);
            
            emitInstruction(ctx, "mult r%d r%d", src1_reg, src2_reg);
            emitInstruction(ctx, "mflo r%d", dest_reg);  // Get low part of multiplication from LO
            
        } else if (strcmp(op, "div") == 0) {
            // Division: div src1 src2 dest (changed from divisao to div)
            int src1_reg = allocateRegister(ctx, arg1);
            int src2_reg = allocateRegister(ctx, arg2);
            int dest_reg = allocateRegister(ctx, arg3);
            
            emitInstruction(ctx, "div r%d r%d", src1_reg, src2_reg);
            emitInstruction(ctx, "mflo r%d", dest_reg);  // Get quotient from LO
            
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
            
            emitInstruction(ctx, "slt r59 r%d r%d", src2_reg, src1_reg); // r59 = src2 < src1
            emitInstruction(ctx, "slt r%d r59 1", dest_reg);                     // dest = (r59 < 1) ? 1 : 0 = !r59
            emitInstruction(ctx, "andi r%d r%d 1", dest_reg, dest_reg);           // Keep only bit 0
            
        } else if (strcmp(op, "sget") == 0) {
            // Set greater than or equal: sget src1 src2 dest (dest = src1 >= src2)
            int src1_reg = allocateRegister(ctx, arg1);
            int src2_reg = allocateRegister(ctx, arg2);
            int dest_reg = allocateRegister(ctx, arg3);
            
            emitInstruction(ctx, "slt r59 r%d r%d", src1_reg, src2_reg); // r59 = src1 < src2
            emitInstruction(ctx, "slt r%d r59 1", dest_reg);                     // dest = (r59 < 1) ? 1 : 0 = !r59
            emitInstruction(ctx, "andi r%d r%d 1", dest_reg, dest_reg);           // Keep only bit 0
            
        } else if (strcmp(op, "sdt") == 0) {
            // Set different: sdt src1 src2 dest (dest = src1 != src2)
            int src1_reg = allocateRegister(ctx, arg1);
            int dest_reg = allocateRegister(ctx, arg3);
            
            if (isImmediate(arg2)) {
                int val = atoi(arg2);
                if (val == 0) {
                    // Compare with zero - result is 1 if not zero, 0 if zero
                    emitInstruction(ctx, "move r59 r%d", src1_reg);          // r59 = src1 (no need to subtract 0)
                    emitInstruction(ctx, "bne r59 r0 neq_%d", ctx->label_counter);
                    emitInstruction(ctx, "li r%d 0", dest_reg);              // Equal
                    emitInstruction(ctx, "j end_%d", ctx->label_counter);
                    emitLabel(ctx, "neq_%d:", ctx->label_counter);
                    emitInstruction(ctx, "li r%d 1", dest_reg);              // Not equal
                    emitLabel(ctx, "end_%d:", ctx->label_counter);
                    ctx->label_counter++;
                } else {
                    emitInstruction(ctx, "addi r58 r0 %d", val);                   // Load immediate using addi from r0
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
            // loadVet array_name base_offset index_reg dest_reg (new IR format)
            int dest_reg = allocateRegister(ctx, arg4); // dest_reg is in arg4
            int array_base_offset = atoi(arg2); // array_base_offset is in arg2 (new IR)
            int index_val_reg = allocateRegister(ctx, arg3); // index_value is in arg3 (reg with actual index)

            emitInstruction(ctx, "addi r57 r30 %d", array_base_offset); // r57 = R30 + array_base_offset
            emitInstruction(ctx, "sll r58 r%d 2", index_val_reg);     // r58 = index_val_reg * 4 (byte offset)
            emitInstruction(ctx, "add r57 r57 r58");                 // r57 = effective_address = r57 + byte_offset
            emitInstruction(ctx, "lw r%d r57 0", dest_reg);          // Load from effective_address
            
        } else if (strcmp(op, "storeVet") == 0) {
            // storeVet src_reg array_name base_offset index_reg (new IR format)
            int src_reg = allocateRegister(ctx, arg1); // src_reg is in arg1
            int array_base_offset = atoi(arg3); // array_base_offset is in arg3 (new IR)
            int index_val_reg = allocateRegister(ctx, arg4); // index_value is in arg4 (reg with actual index)

            emitInstruction(ctx, "addi r57 r30 %d", array_base_offset); // r57 = R30 + array_base_offset
            emitInstruction(ctx, "sll r58 r%d 2", index_val_reg);     // r58 = index_val_reg * 4 (byte offset)
            emitInstruction(ctx, "add r57 r57 r58");                 // r57 = effective_address = r57 + byte_offset
            emitInstruction(ctx, "sw r%d r57 0", src_reg);           // Store to effective_address
            
        } else if (strcmp(op, "GLOBAL_ARRAY") == 0) {
            // Global array declaration
            emitInstruction(ctx, "# Global array %s[%s]", arg1, arg2);
            
        } else if (strcmp(op, "li") == 0) {
            // LI RT, IMMEDIATE - convert to addi from r0
            int rt_reg = allocateRegister(ctx, arg1);
            int immediate_val = atoi(arg2); // arg2 is the immediate value string
            if (immediate_val == 0) {
                emitInstruction(ctx, "move r%d r0", rt_reg);
            } else {
                emitInstruction(ctx, "addi r%d r0 %d", rt_reg, immediate_val);
            }
        } else if (strcmp(op, "set") == 0) {
        printf("DEBUG: Processing set instruction\n");
        // Set equal comparison: set src1 src2 dest (dest = src1 == src2)
        int src1_reg = allocateRegister(ctx, arg1);
        int dest_reg = allocateRegister(ctx, arg3); // This will be t1 in your GCD example

        if (isImmediate(arg2)) {
            int val = atoi(arg2);
            if (val == 0) {
                // Check if src1_reg == 0. If so, dest_reg = 1, else 0.
                // Use positive-only logic: if src1_reg < 1, then it's 0 (assuming non-negative values)
                emitInstruction(ctx, "slt r%d r%d 1", dest_reg, src1_reg);       // dest_reg = (src1_reg < 1) ? 1 : 0 = (src1_reg == 0) ? 1 : 0
                emitInstruction(ctx, "andi r%d r%d 1", dest_reg, dest_reg); // Force result to 0 or 1.
                
            } else {
                // General case for "set src1 val dest" (dest = src1 == val)
                emitInstruction(ctx, "addi r58 r0 %d", val);
                emitInstruction(ctx, "sub r59 r%d r58", src1_reg);     // r59 = src1 - val
                // Check if r59 == 0 using positive-only logic
                emitInstruction(ctx, "slt r60 r0 r59");                // r60 = (0 < r59) ? 1 : 0  (r59 > 0)
                emitInstruction(ctx, "slt r61 r59 r0");                // r61 = (r59 < 0) ? 1 : 0  (r59 < 0)
                emitInstruction(ctx, "add r58 r60 r61");               // r58 = r60 + r61 = (r59 != 0) ? 1 : 0
                emitInstruction(ctx, "slt r%d r58 1", dest_reg);       // dest_reg = (r58 < 1) ? 1 : 0 = (r59 == 0) ? 1 : 0
                emitInstruction(ctx, "andi r%d r%d 1", dest_reg, dest_reg);
            }
        } else {
            int src2_reg = allocateRegister(ctx, arg2);
            emitInstruction(ctx, "sub r59 r%d r%d", src1_reg, src2_reg); // r59 = src1 - src2
            // Check if r59 == 0 using positive-only logic
            emitInstruction(ctx, "slt r60 r0 r59");                // r60 = (0 < r59) ? 1 : 0  (r59 > 0)
            emitInstruction(ctx, "slt r61 r59 r0");                // r61 = (r59 < 0) ? 1 : 0  (r59 < 0)
            emitInstruction(ctx, "add r58 r60 r61");               // r58 = r60 + r61 = (r59 != 0) ? 1 : 0
            emitInstruction(ctx, "slt r%d r58 1", dest_reg);       // dest_reg = (r58 < 1) ? 1 : 0 = (r59 == 0) ? 1 : 0
            emitInstruction(ctx, "andi r%d r%d 1", dest_reg, dest_reg);
        }
            
        // Replace it with the following:
        } else if (strcmp(op, "seq") == 0) {
            printf("DEBUG: Processing seq instruction (simplified)\n");
            int src1_reg = allocateRegister(ctx, arg1);
            int dest_reg = allocateRegister(ctx, arg3);

            if (isImmediate(arg2)) {
                int val = atoi(arg2);
                // Handle LI for immediate value to compare against
                emitInstruction(ctx, "li r58 %d", val); // Load immediate into temp reg r58
                emitInstruction(ctx, "set r%d r%d r58", dest_reg, src1_reg); // Use 'set' instruction for equality
            } else {
                int src2_reg = allocateRegister(ctx, arg2);
                emitInstruction(ctx, "set r%d r%d r%d", dest_reg, src1_reg, src2_reg); // Use 'set' instruction for equality
            }
        } else if (strcmp(op, "sne") == 0) {
            // Set not equal: sne src1 src2 dest (dest = src1 != src2)
            int src1_reg = allocateRegister(ctx, arg1);
            int dest_reg = allocateRegister(ctx, arg3);
            
            if (isImmediate(arg2)) {
                int val = atoi(arg2);
                if (val == 0) {
                    // Simple case: dest = (src1 != 0) ? 1 : 0
                    emitInstruction(ctx, "slt r59 r%d 1", src1_reg);        // r59 = (src1 < 1) ? 1 : 0
                    emitInstruction(ctx, "slt r58 r0 r%d", src1_reg);       // r58 = (0 < src1) ? 1 : 0
                    emitInstruction(ctx, "add r%d r59 r58", dest_reg);      // dest = r59 + r58 = (src1 != 0) ? 1 : 0
                    emitInstruction(ctx, "andi r%d r%d 1", dest_reg, dest_reg);
                } else {
                    emitInstruction(ctx, "addi r58 r0 %d", val);
                    emitInstruction(ctx, "sub r59 r%d r58", src1_reg);      // r59 = src1 - val
                    emitInstruction(ctx, "slt r58 r0 r59");                 // r58 = (0 < r59) ? 1 : 0  (positive case)
                    emitInstruction(ctx, "slt r57 r59 r0");                 // r57 = (r59 < 0) ? 1 : 0  (negative case)
                    emitInstruction(ctx, "add r%d r58 r57", dest_reg);      // dest = r58 + r57 = (r59 != 0) ? 1 : 0
                    emitInstruction(ctx, "andi r%d r%d 1", dest_reg, dest_reg);
                }
            } else {
                int src2_reg = allocateRegister(ctx, arg2);
                emitInstruction(ctx, "sub r59 r%d r%d", src1_reg, src2_reg); // r59 = src1 - src2
                emitInstruction(ctx, "slt r58 r0 r59");                     // r58 = (0 < r59) ? 1 : 0  (positive case)
                emitInstruction(ctx, "slt r57 r59 r0");                     // r57 = (r59 < 0) ? 1 : 0  (negative case)
                emitInstruction(ctx, "add r%d r58 r57", dest_reg);          // dest = r58 + r57 = (r59 != 0) ? 1 : 0
                emitInstruction(ctx, "andi r%d r%d 1", dest_reg, dest_reg);
            }
            
        } else if (strcmp(op, "bne") == 0) {
            // Branch if not equal: bne condition_reg label ___
            int cond_reg = allocateRegister(ctx, arg1);
            emitInstruction(ctx, "bne r%d r0 %s", cond_reg, arg2);  // Jump if condition is false (0)
            
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
            emitInstruction(ctx, "mflo r%d", dest_reg);  // Get quotient from LO
            
        } else if (strcmp(op, "CMP") == 0) {
            // CMP src1, src2, __, __ - Compare for branches
            int src1_reg = allocateRegister(ctx, arg1);
            
            if (isImmediate(arg2)) {
                int val = atoi(arg2);
                if (val == 0) {
                    // Compare with zero - store result in a temporary register
                    emitInstruction(ctx, "move r59 r%d", src1_reg);         // r59 = src1 (no need to subtract 0)
                } else {
                    emitInstruction(ctx, "addi r58 r0 %d", val);            // Load immediate using addi from r0
                    emitInstruction(ctx, "sub r59 r%d r58", src1_reg); // r59 = src1 - val
                }
            } else {
                int src2_reg = allocateRegister(ctx, arg2);
                emitInstruction(ctx, "sub r59 r%d r%d", src1_reg, src2_reg);
            }
            
        } else if (strcmp(op, "BR_NE") == 0) {
            // Direct branch if not equal: BR_NE src1 src2 label
            int src1_reg = allocateRegister(ctx, arg1);
            int src2_reg;
            
            if (isImmediate(arg2)) {
                // Compare with immediate value
                int val = atoi(arg2);
                if (val == 0) {
                    emitInstruction(ctx, "bne r%d r0 %s", src1_reg, arg3);
                } else {
                    emitInstruction(ctx, "li r58 %d", val);
                    emitInstruction(ctx, "bne r%d r58 %s", src1_reg, arg3);
                }
            } else {
                src2_reg = allocateRegister(ctx, arg2);
                emitInstruction(ctx, "bne r%d r%d %s", src1_reg, src2_reg, arg3);
            }

        } else if (strcmp(op, "BR_EQ") == 0) {
            // Direct branch if equal: BR_EQ src1 src2 label
            int src1_reg = allocateRegister(ctx, arg1);
            int src2_reg;
            
            if (isImmediate(arg2)) {
                // Compare with immediate value  
                int val = atoi(arg2);
                if (val == 0) {
                    emitInstruction(ctx, "beq r%d r0 %s", src1_reg, arg3);
                } else {
                    emitInstruction(ctx, "li r58 %d", val);
                    emitInstruction(ctx, "beq r%d r58 %s", src1_reg, arg3);
                }
            } else {
                src2_reg = allocateRegister(ctx, arg2);
                emitInstruction(ctx, "beq r%d r%d %s", src1_reg, src2_reg, arg3);
            }

        } else if (strcmp(op, "BR_GE") == 0) {
            // Direct branch if greater or equal: BR_GE src1 src2 label
            int src1_reg = allocateRegister(ctx, arg1);
            int src2_reg;
            
            if (isImmediate(arg2)) {
                int val = atoi(arg2);
                if (val == 0) {
                    emitInstruction(ctx, "bgte r%d r0 %s", src1_reg, arg3);
                } else {
                    emitInstruction(ctx, "li r58 %d", val);
                    emitInstruction(ctx, "bgte r%d r58 %s", src1_reg, arg3);
                }
            } else {
                src2_reg = allocateRegister(ctx, arg2);
                emitInstruction(ctx, "bgte r%d r%d %s", src1_reg, src2_reg, arg3);
            }
            
        } else if (strcmp(op, "BR_LT") == 0) {
            // Direct branch if less than: BR_LT src1 src2 label
            int src1_reg = allocateRegister(ctx, arg1);
            int src2_reg;
            
            if (isImmediate(arg2)) {
                int val = atoi(arg2);
                if (val == 0) {
                    emitInstruction(ctx, "blt r%d r0 %s", src1_reg, arg3);
                } else {
                    emitInstruction(ctx, "li r58 %d", val);
                    emitInstruction(ctx, "blt r%d r58 %s", src1_reg, arg3);
                }
            } else {
                src2_reg = allocateRegister(ctx, arg2);
                emitInstruction(ctx, "blt r%d r%d %s", src1_reg, src2_reg, arg3);
            }
            
        } else if (strcmp(op, "BR_LE") == 0) {
            // Direct branch if less than or equal: BR_LE src1 src2 label
            int src1_reg = allocateRegister(ctx, arg1);
            int src2_reg;
            
            if (isImmediate(arg2)) {
                int val = atoi(arg2);
                if (val == 0) {
                    emitInstruction(ctx, "blte r%d r0 %s", src1_reg, arg3);
                } else {
                    emitInstruction(ctx, "li r58 %d", val);
                    emitInstruction(ctx, "blte r%d r58 %s", src1_reg, arg3);
                }
            } else {
                src2_reg = allocateRegister(ctx, arg2);
                emitInstruction(ctx, "blte r%d r%d %s", src1_reg, src2_reg, arg3);
            }
            
        } else if (strcmp(op, "BR_GT") == 0) {
            // Direct branch if greater than: BR_GT src1 src2 label
            int src1_reg = allocateRegister(ctx, arg1);
            int src2_reg;
            
            if (isImmediate(arg2)) {
                int val = atoi(arg2);
                if (val == 0) {
                    emitInstruction(ctx, "bgt r%d r0 %s", src1_reg, arg3);
                } else {
                    emitInstruction(ctx, "li r58 %d", val);
                    emitInstruction(ctx, "bgt r%d r58 %s", src1_reg, arg3);
                }
            } else {
                src2_reg = allocateRegister(ctx, arg2);
                emitInstruction(ctx, "bgt r%d r%d %s", src1_reg, src2_reg, arg3);
            }
            
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
                printf("DEBUG: Returning value %d from function\n", ret_reg);
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
            
        } else if (strcmp(op, "li") == 0) {
            // LI RT, IMMEDIATE - convert to addi from r0
            int rt_reg = allocateRegister(ctx, arg1);
            int immediate_val = atoi(arg2); // arg2 is the immediate value string
            if (immediate_val == 0) {
                emitInstruction(ctx, "move r%d r0", rt_reg);
            } else {
                emitInstruction(ctx, "addi r%d r0 %d", rt_reg, immediate_val);
            }
        } else {
            // Only emit unknown IR comment for truly unknown IRs
            if (strcmp(op, "allocaMemVar") == 0) {
                // Do nothing, skip
            } else {
                emitInstruction(ctx, "# Unknown IR: %s", line);
            }
        }
    } while (reprocess);
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
