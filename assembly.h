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
    // Arithmetic and Logic Instructions
    INSTR_ADD,      // Add (000000)
    INSTR_SUB,      // Subtract (000001)
    INSTR_MULT,     // Multiply (000010)
    INSTR_DIV,      // Divide (000011)
    INSTR_AND,      // Bitwise AND (000100)
    INSTR_OR,       // Bitwise OR (000101)
    INSTR_SLL,      // Shift left logical (000110)
    INSTR_SRL,      // Shift right logical (000111)
    INSTR_SLT,      // Set less than (001000)
    
    // Move Instructions
    INSTR_MFHI,     // Move from HI register (001001)
    INSTR_MFLO,     // Move from LO register (001010)
    INSTR_MOVE,     // Move register (001011)
    
    // Jump Instructions
    INSTR_JR,       // Jump register (001100)
    INSTR_JALR,     // Jump and link register (001101)
    INSTR_J,        // Jump (011100)
    INSTR_JAL,      // Jump and link (011101)
    
    // Immediate Instructions
    INSTR_LA,       // Load address (001110)
    INSTR_ADDI,     // Add immediate (001111)
    INSTR_SUBI,     // Subtract immediate (010000)
    INSTR_ANDI,     // AND immediate (010001)
    INSTR_ORI,      // OR immediate (010010)
    INSTR_LI,       // Load immediate (011011)
    
    // Branch Instructions
    INSTR_BEQ,      // Branch if equal (010011)
    INSTR_BNE,      // Branch if not equal (010100)
    INSTR_BGT,      // Branch if greater than (010101)
    INSTR_BGTE,     // Branch if greater than or equal (010110)
    INSTR_BLT,      // Branch if less than (010111)
    INSTR_BLTE,     // Branch if less than or equal (011000)
    INSTR_BEQZ,     // Branch if equal to zero (custom)
    
    // Memory Instructions
    INSTR_LW,       // Load word (011001)
    INSTR_SW,       // Store word (011010)
    
    // I/O Instructions
    INSTR_OUTPUTMEM,  // Output memory (011111)
    INSTR_OUTPUTREG,  // Output register (100000)
    INSTR_OUTPUTRESET, // Output reset (100001)
    INSTR_INPUT,      // Input (100010)
    
    // Control Instructions
    INSTR_HALT,     // Halt processor (011110)
    
    // Legacy/Compatibility
    INSTR_JUMP,     // Legacy jump (mapped to J)
    INSTR_SETI,     // Legacy set immediate (mapped to LI)
    INSTR_OUTPUT    // Legacy output (mapped to OUTPUTREG)
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
