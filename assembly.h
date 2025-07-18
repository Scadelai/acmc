#ifndef _ASSEMBLY_H_
#define _ASSEMBLY_H_

#include "globals.h"
#include <stdio.h>

// Maximum limits for assembly generation
#ifndef MAX_REGISTERS
#define MAX_REGISTERS 32
#endif
#define MAX_INSTRUCTIONS 1000
#define MAX_FUNCTIONS 50
#define MAX_VARIABLES 200
#ifndef MAX_LABEL_LEN
#define MAX_LABEL_LEN 50
#endif

// Register definitions (MIPS-like RISC architecture)
typedef enum {
    R0 = 0,   // Zero register
    R1,       // General purpose registers
    R2, R3, R4, R5, R6, R7, R8, R9, R10,
    R11, R12, R13, R14, R15, R16, R17, R18, R19, R20,
    R21, R22, R23, R24, R25, R26, R27,
    R28,      // Function return value register
    R29,      // Frame pointer ($fp)
    R30,      // Stack pointer ($sp)
    R31       // Return address ($ra)
} RegisterType;

// Assembly instruction types
typedef enum {
    INSTR_ADD,    // Add
    INSTR_ADDI,   // Add immediate
    INSTR_SUB,    // Subtract
    INSTR_MULT,   // Multiply
    INSTR_DIV,    // Divide
    INSTR_LW,     // Load word
    INSTR_SW,     // Store word
    INSTR_SETI,   // Set immediate
    INSTR_BEQZ,   // Branch if equal to zero
    INSTR_JUMP,   // Jump
    INSTR_JAL,    // Jump and link
    INSTR_JR,     // Jump register
    INSTR_INPUT,  // Input operation
    INSTR_OUTPUT  // Output operation
} InstructionType;

// Assembly instruction structure
typedef struct {
    InstructionType op;
    RegisterType rs, rt, rd;
    int immediate;
    char label[MAX_LABEL_LEN];
    int line_number;
} AssemblyInstruction;

// Variable information for assembly generation
typedef struct Variable {
    char name[64];
    int scope_level;     // 0 = global, >0 = local
    int memory_offset;   // Memory location/offset
    int size;           // 1 for int, >1 for arrays
    struct Variable *next;
} Variable;

// Function scope information
typedef struct FunctionScope {
    char name[64];
    int local_vars_count;
    int params_count;
    int memory_size;
    Variable *variables;
    struct FunctionScope *next;
} FunctionScope;

// Assembly generation context
typedef struct {
    FILE *output_file;
    AssemblyInstruction instructions[MAX_INSTRUCTIONS];
    int instruction_count;
    FunctionScope *functions;
    Variable *global_vars;
    int current_label_num;
    int register_usage[MAX_REGISTERS];  // 0 = free, 1 = used
    FunctionScope *current_function;
} AssemblyContext;

// Main assembly generation functions
void generateAssembly(const char *ir_file, const char *assembly_file);
void generateAssemblyFromIRImproved(const char *ir_file, const char *assembly_file);
AssemblyContext* initAssemblyContext(FILE *output);
void destroyAssemblyContext(AssemblyContext *ctx);

// Instruction generation functions
void emitInstruction(AssemblyContext *ctx, InstructionType op, 
                    RegisterType rs, RegisterType rt, RegisterType rd, 
                    int immediate, const char *label);
void emitLabel(AssemblyContext *ctx, const char *label);
void emitJump(AssemblyContext *ctx, int target_line);
void emitFunctionLabel(AssemblyContext *ctx, const char *func_name);

// Register management
RegisterType allocateRegister(AssemblyContext *ctx);
void freeRegister(AssemblyContext *ctx, RegisterType reg);
void saveRegisters(AssemblyContext *ctx);
void restoreRegisters(AssemblyContext *ctx);

// Variable and scope management
Variable* findVariable(AssemblyContext *ctx, const char *name);
FunctionScope* findFunction(AssemblyContext *ctx, const char *name);
void addVariable(AssemblyContext *ctx, const char *name, int scope_level, int size);
void addFunction(AssemblyContext *ctx, const char *name);

// IR parsing and translation
void parseIRLine(AssemblyContext *ctx, const char *line);
void translateIRToAssembly(AssemblyContext *ctx, const char *ir_file);

// Utility functions
const char* getRegisterName(RegisterType reg);
const char* getInstructionName(InstructionType instr);
int isTemporaryRegister(const char *name);
RegisterType getRegisterFromName(const char *name);

#endif
