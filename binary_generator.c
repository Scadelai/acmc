/*
 * binary_generator.c - Convert assembly to binary representation
 * 
 * Converts ACMC assembly code to 32-bit binary instructions following
 * the custom MIPS processor specification from instrucoes_processador.md
 * 
 * Architecture Analysis from Quartus Project:
 * - 64 registers (R0-R63): R62=LO, R63=HI, R31=return address, R0=zero
 * - 32-bit instructions with 6-bit opcodes
 * - Memory addressing: 14-bit immediate, 6-bit addresses for jumps
 * - Stack-based function calls with R30 as stack pointer
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

// Instruction formats from processor specification
typedef enum {
    FORMAT_R = 0,  // R-type: OPCODE | RS | RT | RD | unused
    FORMAT_I = 1,  // I-type: OPCODE | RS | RT | IMMEDIATE(14-bit)
    FORMAT_J = 2   // J-type: OPCODE | ADDRESS(6-bit) | unused
} InstructionFormat;

// Processor instruction definition
typedef struct {
    char mnemonic[16];
    uint8_t opcode;
    InstructionFormat format;
    char description[64];
} ProcessorInstruction;

// Complete instruction set from instrucoes_processador.md with correct binary opcodes
static ProcessorInstruction instructions[] = {
    // Arithmetic and Logic (R-type)
    {"add",        0x00, FORMAT_R, "ADD RD, RS, RT"},        // 000000
    {"sub",        0x01, FORMAT_R, "SUB RD, RS, RT"},        // 000001
    {"mult",       0x02, FORMAT_R, "MULT RS, RT"},           // 000010
    {"div",        0x03, FORMAT_R, "DIV RS, RT"},            // 000011
    {"and",        0x04, FORMAT_R, "AND RD, RS, RT"},        // 000100
    {"or",         0x05, FORMAT_R, "OR RD, RS, RT"},         // 000101
    {"sll",        0x06, FORMAT_R, "SLL RD, RS, SHAMT"},     // 000110
    {"srl",        0x07, FORMAT_R, "SRL RD, RS, SHAMT"},     // 000111
    {"slt",        0x08, FORMAT_R, "SLT RD, RS, RT"},        // 001000
    
    // Movement (R-type)
    {"mfhi",       0x09, FORMAT_R, "MFHI RD"},               // 001001
    {"mflo",       0x0A, FORMAT_R, "MFLO RD"},               // 001010
    {"move",       0x0B, FORMAT_R, "MOVE RD, RS"},           // 001011
    
    // Jump Register (R-type)
    {"jr",         0x0C, FORMAT_R, "JR RS"},                 // 001100
    {"jalr",       0x0D, FORMAT_R, "JALR RS"},               // 001101
    
    // Load Address (I-type)
    {"la",         0x0E, FORMAT_I, "LA RT, ADDRESS"},        // 001110
    
    // Immediate operations (I-type)
    {"addi",       0x0F, FORMAT_I, "ADDI RT, RS, IMMEDIATE"}, // 001111
    {"subi",       0x10, FORMAT_I, "SUBI RT, RS, IMMEDIATE"}, // 010000
    {"andi",       0x11, FORMAT_I, "ANDI RT, RS, IMMEDIATE"}, // 010001
    {"ori",        0x12, FORMAT_I, "ORI RT, RS, IMMEDIATE"},  // 010010
    
    // Branch instructions (I-type with address in [5:0])
    {"beq",        0x13, FORMAT_I, "BEQ RS, RT, ADDRESS"},   // 010011
    {"bne",        0x14, FORMAT_I, "BNE RS, RT, ADDRESS"},   // 010100
    {"bgt",        0x15, FORMAT_I, "BGT RS, RT, ADDRESS"},   // 010101
    {"bgte",       0x16, FORMAT_I, "BGTE RS, RT, ADDRESS"},  // 010110
    {"blt",        0x17, FORMAT_I, "BLT RS, RT, ADDRESS"},   // 010111
    {"blte",       0x18, FORMAT_I, "BLTE RS, RT, ADDRESS"},  // 011000
    
    // Memory operations (I-type)
    {"lw",         0x19, FORMAT_I, "LW RT, OFFSET(RS)"},     // 011001
    {"sw",         0x1A, FORMAT_I, "SW RT, OFFSET(RS)"},     // 011010
    {"li",         0x1B, FORMAT_I, "LI RT, IMMEDIATE"},      // 011011
    
    // Jump instructions (J-type)
    {"j",          0x1C, FORMAT_J, "J ADDRESS"},             // 011100
    {"jal",        0x1D, FORMAT_J, "JAL ADDRESS"},           // 011101
    
    // Control (R-type)
    {"halt",       0x1E, FORMAT_R, "HALT"},                  // 011110
    
    // I/O instructions
    {"outputmem",  0x1F, FORMAT_I, "OUTPUTMEM RS, ADDRESS"}, // 011111
    {"outputreg",  0x20, FORMAT_R, "OUTPUTREG RS"},          // 100000
    {"outputreset",0x21, FORMAT_R, "OUTPUT RESET"},          // 100001
    {"input",      0x22, FORMAT_R, "INPUT RD"},              // 100010
};

#define NUM_INSTRUCTIONS (sizeof(instructions) / sizeof(instructions[0]))

// Label storage for address resolution
typedef struct {
    char name[64];
    uint32_t address;
} Label;

static Label labels[256];
static int label_count = 0;

// Parse register number from string (e.g., "r5" -> 5, "R31" -> 31)
int parseRegister(const char *reg_str) {
    if (!reg_str || strlen(reg_str) < 2) return 0;
    
    if (reg_str[0] == 'r' || reg_str[0] == 'R') {
        return atoi(&reg_str[1]);
    }
    
    return 0;
}

// Parse immediate value or address
int parseImmediate(const char *imm_str) {
    if (!imm_str) return 0;
    
    // Check if it's a label reference
    for (int i = 0; i < label_count; i++) {
        if (strcmp(labels[i].name, imm_str) == 0) {
            return labels[i].address;
        }
    }
    
    // Parse as number
    return atoi(imm_str);
}

// Find instruction by mnemonic
ProcessorInstruction* findInstruction(const char *mnemonic) {
    for (int i = 0; i < NUM_INSTRUCTIONS; i++) {
        if (strcasecmp(instructions[i].mnemonic, mnemonic) == 0) {
            return &instructions[i];
        }
    }
    return NULL;
}

// Generate binary for R-type instruction
uint32_t generateRType(ProcessorInstruction *instr, int rs, int rt, int rd, int shamt) {
    uint32_t binary = 0;
    
    // Format: [31:26] OPCODE | [25:20] RS | [19:14] RT | [13:8] RD | [7:0] SHAMT/unused
    binary |= ((uint32_t)instr->opcode & 0x3F) << 26;  // OPCODE [31:26]
    binary |= ((uint32_t)rs & 0x3F) << 20;             // RS [25:20]
    binary |= ((uint32_t)rt & 0x3F) << 14;             // RT [19:14]
    binary |= ((uint32_t)rd & 0x3F) << 8;              // RD [13:8]
    binary |= ((uint32_t)shamt & 0xFF);                // SHAMT/unused [7:0]
    
    return binary;
}

// Generate binary for I-type instruction
uint32_t generateIType(ProcessorInstruction *instr, int rs, int rt, int immediate) {
    uint32_t binary = 0;
    
    // Special handling for branch instructions - address goes in [5:0]
    if (strcmp(instr->mnemonic, "beq") == 0 || strcmp(instr->mnemonic, "bne") == 0 ||
        strcmp(instr->mnemonic, "bgt") == 0 || strcmp(instr->mnemonic, "bgte") == 0 ||
        strcmp(instr->mnemonic, "blt") == 0 || strcmp(instr->mnemonic, "blte") == 0) {
        // Branch format: [31:26] OPCODE | [25:20] RS | [19:14] RT | [13:6] unused | [5:0] ADDRESS
        binary |= ((uint32_t)instr->opcode & 0x3F) << 26;  // OPCODE [31:26]
        binary |= ((uint32_t)rs & 0x3F) << 20;             // RS [25:20]
        binary |= ((uint32_t)rt & 0x3F) << 14;             // RT [19:14]
        binary |= ((uint32_t)immediate & 0x3F);            // ADDRESS [5:0]
    } else {
        // Regular I-type format: [31:26] OPCODE | [25:20] RS | [19:14] RT | [13:0] IMMEDIATE
        binary |= ((uint32_t)instr->opcode & 0x3F) << 26;  // OPCODE [31:26]
        binary |= ((uint32_t)rs & 0x3F) << 20;             // RS [25:20]
        binary |= ((uint32_t)rt & 0x3F) << 14;             // RT [19:14]
        binary |= ((uint32_t)immediate & 0x3FFF);          // IMMEDIATE [13:0]
    }
    
    return binary;
}

// Generate binary for J-type instruction
uint32_t generateJType(ProcessorInstruction *instr, int address) {
    uint32_t binary = 0;
    
    // Format: [31:26] OPCODE | [25:0] ADDRESS (but spec says [5:0] ADDRESS in low bits)
    // Following spec: [31:26] OPCODE | [25:6] unused | [5:0] ADDRESS
    binary |= ((uint32_t)instr->opcode & 0x3F) << 26;  // OPCODE [31:26]
    binary |= ((uint32_t)address & 0x3F);              // ADDRESS [5:0]
    
    return binary;
}

// Parse assembly instruction and generate binary
uint32_t parseInstruction(const char *line, uint32_t pc) {
    char instr_copy[256];
    strcpy(instr_copy, line);
    
    // Remove instruction number prefix (e.g., "5-add" -> "add")
    char *dash = strchr(instr_copy, '-');
    if (dash) {
        memmove(instr_copy, dash + 1, strlen(dash));
    }
    
    // Check if this is a numbered comment line (e.g., "3-# Parameter u in r1")
    // These should be treated as instruction slots with NOP
    if (instr_copy[0] == '#') {
        return 0; // NOP instruction (all zeros)
    }
    
    // Skip function headers and other non-numbered comments
    if (strstr(instr_copy, "Func") || strstr(instr_copy, "CEHOLDER")) {
        return 0xFFFFFFFF; // Special marker for non-instruction lines
    }
    
    // Skip labels (lines ending with ':')
    if (strchr(instr_copy, ':')) {
        return 0xFFFFFFFF; // Skip label definitions
    }
    
    // Skip single letter lines like "R" which are assembly artifacts
    if (strlen(instr_copy) == 1 || strlen(instr_copy) == 2) {
        return 0xFFFFFFFF; // Skip short non-instruction lines
    }
    
    // Tokenize instruction
    char *tokens[8];
    int token_count = 0;
    
    char *token = strtok(instr_copy, " \t,");
    while (token && token_count < 8) {
        tokens[token_count++] = token;
        token = strtok(NULL, " \t,");
    }
    
    if (token_count == 0) return 0;
    
    ProcessorInstruction *instr = findInstruction(tokens[0]);
    if (!instr) {
        printf("Warning: Unknown instruction '%s'\n", tokens[0]);
        return 0;
    }
    
    switch (instr->format) {
        case FORMAT_R: {
            int rs = 0, rt = 0, rd = 0, shamt = 0;
            
            if (strcmp(instr->mnemonic, "add") == 0 || strcmp(instr->mnemonic, "sub") == 0 ||
                strcmp(instr->mnemonic, "and") == 0 || strcmp(instr->mnemonic, "or") == 0 ||
                strcmp(instr->mnemonic, "slt") == 0) {
                // Format: ADD RD, RS, RT
                if (token_count >= 4) {
                    rd = parseRegister(tokens[1]);
                    rs = parseRegister(tokens[2]);
                    rt = parseRegister(tokens[3]);
                }
            } else if (strcmp(instr->mnemonic, "mult") == 0 || strcmp(instr->mnemonic, "div") == 0) {
                // Format: MULT RS, RT
                if (token_count >= 3) {
                    rs = parseRegister(tokens[1]);
                    rt = parseRegister(tokens[2]);
                }
            } else if (strcmp(instr->mnemonic, "move") == 0) {
                // Format: MOVE RD, RS
                if (token_count >= 3) {
                    rd = parseRegister(tokens[1]);
                    rs = parseRegister(tokens[2]);
                }
            } else if (strcmp(instr->mnemonic, "mfhi") == 0 || strcmp(instr->mnemonic, "mflo") == 0) {
                // Format: MFHI RD
                if (token_count >= 2) {
                    rd = parseRegister(tokens[1]);
                }
            } else if (strcmp(instr->mnemonic, "jr") == 0 || strcmp(instr->mnemonic, "jalr") == 0) {
                // Format: JR RS
                if (token_count >= 2) {
                    rs = parseRegister(tokens[1]);
                }
            } else if (strcmp(instr->mnemonic, "sll") == 0 || strcmp(instr->mnemonic, "srl") == 0) {
                // Format: SLL RD, RS, SHAMT
                if (token_count >= 4) {
                    rd = parseRegister(tokens[1]);
                    rs = parseRegister(tokens[2]);
                    shamt = parseImmediate(tokens[3]);
                }
            } else if (strcmp(instr->mnemonic, "outputreg") == 0) {
                // Format: OUTPUTREG RS
                if (token_count >= 2) {
                    rs = parseRegister(tokens[1]);
                }
            } else if (strcmp(instr->mnemonic, "input") == 0) {
                // Format: INPUT RD
                if (token_count >= 2) {
                    rd = parseRegister(tokens[1]);
                }
            }
            
            return generateRType(instr, rs, rt, rd, shamt);
        }
        
        case FORMAT_I: {
            int rs = 0, rt = 0, immediate = 0;
            
            if (strcmp(instr->mnemonic, "addi") == 0 || strcmp(instr->mnemonic, "subi") == 0 ||
                strcmp(instr->mnemonic, "andi") == 0 || strcmp(instr->mnemonic, "ori") == 0) {
                // Format: ADDI RT, RS, IMMEDIATE
                if (token_count >= 4) {
                    rt = parseRegister(tokens[1]);
                    rs = parseRegister(tokens[2]);
                    immediate = parseImmediate(tokens[3]);
                }
            } else if (strcmp(instr->mnemonic, "li") == 0) {
                // Format: LI RT, IMMEDIATE
                if (token_count >= 3) {
                    rt = parseRegister(tokens[1]);
                    immediate = parseImmediate(tokens[2]);
                }
            } else if (strcmp(instr->mnemonic, "la") == 0) {
                // Format: LA RT, ADDRESS (RT in [19:14] position)
                if (token_count >= 3) {
                    rt = parseRegister(tokens[1]);
                    immediate = parseImmediate(tokens[2]);
                }
            } else if (strstr(instr->mnemonic, "b") == instr->mnemonic) {
                // Branch instructions: BEQ RS, RT, ADDRESS
                if (token_count >= 4) {
                    rs = parseRegister(tokens[1]);
                    rt = parseRegister(tokens[2]);
                    immediate = parseImmediate(tokens[3]);
                }
            } else if (strcmp(instr->mnemonic, "lw") == 0 || strcmp(instr->mnemonic, "sw") == 0) {
                // Format: LW RT, OFFSET(RS) or LW RT RS OFFSET
                if (token_count >= 4) {
                    rt = parseRegister(tokens[1]);
                    rs = parseRegister(tokens[2]);
                    immediate = parseImmediate(tokens[3]);
                } else if (token_count >= 3) {
                    // Handle formats like "lw r28 r30 1"
                    rt = parseRegister(tokens[1]);
                    rs = parseRegister(tokens[2]);
                    if (token_count > 3) {
                        immediate = parseImmediate(tokens[3]);
                    }
                }
            } else if (strcmp(instr->mnemonic, "outputmem") == 0) {
                // Format: OUTPUTMEM RS, ADDRESS
                if (token_count >= 3) {
                    rs = parseRegister(tokens[1]);
                    immediate = parseImmediate(tokens[2]);
                }
            }
            
            return generateIType(instr, rs, rt, immediate);
        }
        
        case FORMAT_J: {
            int address = 0;
            
            if (token_count >= 2) {
                address = parseImmediate(tokens[1]);
            }
            
            return generateJType(instr, address);
        }
    }
    
    return 0;
}

// First pass: collect labels
void collectLabels(FILE *asm_file) {
    char line[512];
    uint32_t pc = 0;
    
    while (fgets(line, sizeof(line), asm_file)) {
        // Remove newline
        char *newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        // Skip empty lines and whitespace
        char *trimmed_line = line;
        while (*trimmed_line == ' ' || *trimmed_line == '\t') trimmed_line++;
        if (*trimmed_line == '\0') continue;
        
        // Check for numbered instructions to track pc properly
        char *dash = strchr(trimmed_line, '-');
        if (dash) {
            // Extract instruction number
            char *number_end = dash;
            *number_end = '\0';
            int instruction_number = atoi(trimmed_line);
            *number_end = '-'; // Restore the dash
            
            char *after_dash = dash + 1;
            while (*after_dash == ' ' || *after_dash == '\t') after_dash++;
            
            // Update pc to match instruction number
            if (!strstr(after_dash, "Func") && !strstr(after_dash, "CEHOLDER") && 
                !strchr(after_dash, ':') && after_dash[0] != '#') {
                pc = instruction_number;
            }
        }
        
        // Check for function definitions (e.g., "Func gcd:")
        if (strstr(trimmed_line, "Func ") && strchr(trimmed_line, ':')) {
            char *func_start = strstr(trimmed_line, "Func ");
            func_start += 5; // Skip "Func "
            while (*func_start == ' ') func_start++;
            
            char *colon = strchr(func_start, ':');
            if (colon) {
                *colon = '\0';
                strcpy(labels[label_count].name, func_start);
                labels[label_count].address = pc; // Next instruction will be at this PC
                label_count++;
            }
        }
        
        // Check for label definitions (simple labels like "L0:", "equal_0:", etc.)
        else if (strchr(trimmed_line, ':') && !strstr(trimmed_line, "Func")) {
            char *colon = strchr(trimmed_line, ':');
            *colon = '\0';
            
            // Remove any number prefix (e.g., "10-equal_0:" -> "equal_0")
            char *dash_in_label = strchr(trimmed_line, '-');
            char *label_name = dash_in_label ? dash_in_label + 1 : trimmed_line;
            while (*label_name == ' ' || *label_name == '\t') label_name++;
            
            strcpy(labels[label_count].name, label_name);
            labels[label_count].address = pc + 1; // Points to next instruction after current pc
            label_count++;
        }
        
        // For non-numbered lines like "j 39", increment pc
        if (!dash && trimmed_line[0] != '#' && !strstr(trimmed_line, "Func") && 
            !strstr(trimmed_line, "CEHOLDER") && !strchr(trimmed_line, ':')) {
            pc++;
        }
    }
    
    rewind(asm_file);
}

// Print binary in formatted style (with comments)
void printBinaryWithComments(FILE *output, uint32_t binary, uint32_t address, const char *original_line) {
    // Print 32-bit binary with field separators
    fprintf(output, "# Address %u: %s\n", address, original_line);
    
    // Extract fields for documentation
    uint8_t opcode = (binary >> 26) & 0x3F;
    uint8_t rs = (binary >> 20) & 0x3F;
    uint8_t rt = (binary >> 14) & 0x3F;
    uint8_t rd = (binary >> 8) & 0x3F;
    uint16_t immediate = binary & 0x3FFF;
    uint8_t address_field = binary & 0x3F;
    
    fprintf(output, "# OPCODE=%06b", opcode);
    
    // Find instruction name for better documentation
    for (int i = 0; i < NUM_INSTRUCTIONS; i++) {
        if (instructions[i].opcode == opcode) {
            fprintf(output, " (%s)", instructions[i].mnemonic);
            
            if (instructions[i].format == FORMAT_R) {
                fprintf(output, ", RS=R%u, RT=R%u, RD=R%u", rs, rt, rd);
            } else if (instructions[i].format == FORMAT_I) {
                if (strstr(instructions[i].mnemonic, "b") == instructions[i].mnemonic) {
                    fprintf(output, ", RS=R%u, RT=R%u, ADDR=%u", rs, rt, address_field);
                } else {
                    fprintf(output, ", RS=R%u, RT=R%u, IMM=%u", rs, rt, immediate);
                }
            } else if (instructions[i].format == FORMAT_J) {
                fprintf(output, ", ADDR=%u", address_field);
            }
            break;
        }
    }
    
    fprintf(output, "\n");
    
    // Print binary with field separators
    for (int i = 31; i >= 0; i--) {
        fprintf(output, "%d", (binary >> i) & 1);
        if (i == 26 || i == 20 || i == 14 || i == 8 || i == 6) {
            fprintf(output, " ");
        }
    }
    fprintf(output, "\n\n");
}

// Print binary in clean format (no comments)
void printBinaryClean(FILE *output, uint32_t binary) {
    // Print 32-bit binary as single line
    for (int i = 31; i >= 0; i--) {
        fprintf(output, "%d", (binary >> i) & 1);
    }
    fprintf(output, "\n");
}

// Main binary generation function - generates both formats
void generateBinaryFromAssembly(const char *asm_filename, const char *clean_bin_filename, const char *commented_bin_filename) {
    FILE *asm_file = fopen(asm_filename, "r");
    if (!asm_file) {
        printf("Error: Cannot open assembly file %s\n", asm_filename);
        return;
    }
    
    FILE *clean_bin_file = fopen(clean_bin_filename, "w");
    if (!clean_bin_file) {
        printf("Error: Cannot create clean binary file %s\n", clean_bin_filename);
        fclose(asm_file);
        return;
    }
    
    FILE *commented_bin_file = fopen(commented_bin_filename, "w");
    if (!commented_bin_file) {
        printf("Error: Cannot create commented binary file %s\n", commented_bin_filename);
        fclose(asm_file);
        fclose(clean_bin_file);
        return;
    }
    
    // Header for commented version
    fprintf(commented_bin_file, "# Binary representation of %s\n", asm_filename);
    fprintf(commented_bin_file, "# Format: [31:26] OPCODE | [25:20] RS | [19:14] RT | [13:8] RD | [7:0] IMEDIATO/ENDEREÇO\n");
    fprintf(commented_bin_file, "# Architecture: Custom MIPS with 64 registers, 32-bit instructions\n");
    fprintf(commented_bin_file, "# Special Registers: R0=zero, R31=return, R62=LO, R63=HI, R30=stack\n\n");
    
    // First pass: collect labels
    label_count = 0;
    collectLabels(asm_file);
    
    printf("Collected %d labels:\n", label_count);
    for (int i = 0; i < label_count; i++) {
        printf("  %s -> address %u\n", labels[i].name, labels[i].address);
    }
    
    // Second pass: generate binary
    char line[512];
    uint32_t pc = 0;
    
    while (fgets(line, sizeof(line), asm_file)) {
        // Remove newline
        char *newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        // Skip empty lines
        if (strlen(line) == 0) continue;
        
        // Store original line for documentation
        char original_line[512];
        strcpy(original_line, line);
        
        // Generate binary for instruction
        uint32_t binary = parseInstruction(line, pc);
        
        if (binary == 0xFFFFFFFF) {
            // Skip non-instruction lines (function headers, labels) - don't increment PC
            continue;
        } else if (binary == 0) {
            // Check if this is really a numbered comment or just a malformed line
            char *dash = strchr(line, '-');
            if (dash) {
                // This is a numbered line (even if comment) - should count
                printBinaryWithComments(commented_bin_file, 0, pc, original_line);
                printBinaryClean(clean_bin_file, 0);
                pc++;
            } else {
                // Non-numbered line that parsed as 0 - just skip
                continue;
            }
        } else {
            // Real instruction
            printBinaryWithComments(commented_bin_file, binary, pc, original_line);
            printBinaryClean(clean_bin_file, binary);
            pc++;
        }
    }
    
    fclose(asm_file);
    fclose(clean_bin_file);
    fclose(commented_bin_file);
    
    printf("Binary generation completed:\n");
    printf("  Clean binary: %s\n", clean_bin_filename);
    printf("  Commented binary: %s\n", commented_bin_filename);
    printf("Generated %u binary instructions\n", pc);
}

// Convenience function with automatic naming
void generateBinaryWithAutoNaming(const char *base_filename) {
    // Extract base name without extension
    char base_name[256];
    strcpy(base_name, base_filename);
    char *dot = strrchr(base_name, '.');
    if (dot) *dot = '\0';
    
    // Generate filenames
    char clean_filename[300];
    char commented_filename[300];
    snprintf(clean_filename, sizeof(clean_filename), "%s.bin", base_name);
    snprintf(commented_filename, sizeof(commented_filename), "%s.binbd", base_name);
    
    generateBinaryFromAssembly(base_filename, clean_filename, commented_filename);
}

// End of binary_generator.c - integrated as library
