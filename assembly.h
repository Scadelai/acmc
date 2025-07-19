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
#define MAX_LABELS 100
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

// Label tracking structure
typedef struct {
    char name[MAX_LABEL_LEN];
    int address;
    int defined;
} LabelInfo;

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

// Generic register mapping for any C- program
typedef struct {
    char ir_name[32];     // IR variable name (e.g., "R1", "u", "v", "i", "x")
    int phys_reg;         // Physical register number (0-63)
    int valid;            // 1 if mapping is active
    int is_param;         // 1 if this is a function parameter
    int is_global;        // 1 if this is a global variable
} RegisterMapping;

// Assembly generation context - generic for any C- program
typedef struct {
    FILE *output;
    int instruction_count;
    RegisterMapping reg_map[128];  // Support many variables
    int next_temp_reg;             // Next available temporary register
    char current_function[64];     // Current function name
    int label_counter;             // For generating unique labels
    int param_counter;             // Track parameter order in current function
} AssemblyContext;

// Main assembly generation functions
void generateAssemblyFromIRImproved(const char *ir_file, const char *assembly_file);
void initializeContext(AssemblyContext *ctx, FILE *output);

// Core assembly generation functions
void processIRLine(AssemblyContext *ctx, const char *line);
int allocateRegister(AssemblyContext *ctx, const char *var_name);
void resetFunctionContext(AssemblyContext *ctx, const char *func_name);
int isImmediate(const char *str);
void emitInstruction(AssemblyContext *ctx, const char *format, ...);
void emitFunctionLabel(AssemblyContext *ctx, const char *func_name);

#endif
