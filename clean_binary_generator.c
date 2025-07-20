/*
 * clean_binary_generator.c - Generate clean binary without comments
 * 
 * Creates a minimal binary file with just the 32-bit binary numbers,
 * one per line, no comments, no field separators, no documentation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

// Reuse the same instruction structures and parsing logic
typedef enum {
    FORMAT_R = 0,
    FORMAT_I = 1,
    FORMAT_J = 2
} InstructionFormat;

typedef struct {
    char mnemonic[16];
    uint8_t opcode;
    InstructionFormat format;
} ProcessorInstruction;

static ProcessorInstruction instructions[] = {
    {"add",        0x00, FORMAT_R}, {"sub",        0x01, FORMAT_R},
    {"mult",       0x02, FORMAT_R}, {"div",        0x03, FORMAT_R},
    {"and",        0x04, FORMAT_R}, {"or",         0x05, FORMAT_R},
    {"sll",        0x06, FORMAT_R}, {"srl",        0x07, FORMAT_R},
    {"slt",        0x08, FORMAT_R}, {"mfhi",       0x09, FORMAT_R},
    {"mflo",       0x0A, FORMAT_R}, {"move",       0x0B, FORMAT_R},
    {"jr",         0x0C, FORMAT_R}, {"jalr",       0x0D, FORMAT_R},
    {"la",         0x0E, FORMAT_I}, {"addi",       0x0F, FORMAT_I},
    {"subi",       0x10, FORMAT_I}, {"andi",       0x11, FORMAT_I},
    {"ori",        0x12, FORMAT_I}, {"beq",        0x13, FORMAT_I},
    {"bne",        0x14, FORMAT_I}, {"bgt",        0x15, FORMAT_I},
    {"bgte",       0x16, FORMAT_I}, {"blt",        0x17, FORMAT_I},
    {"blte",       0x18, FORMAT_I}, {"lw",         0x19, FORMAT_I},
    {"sw",         0x1A, FORMAT_I}, {"li",         0x1B, FORMAT_I},
    {"j",          0x1C, FORMAT_J}, {"jal",        0x1D, FORMAT_J},
    {"halt",       0x1E, FORMAT_R}, {"outputmem",  0x1F, FORMAT_I},
    {"outputreg",  0x20, FORMAT_R}, {"outputreset",0x21, FORMAT_R},
    {"input",      0x22, FORMAT_R},
};

#define NUM_INSTRUCTIONS (sizeof(instructions) / sizeof(instructions[0]))

typedef struct {
    char name[64];
    uint32_t address;
} Label;

static Label labels[256];
static int label_count = 0;

int parseRegister(const char *reg_str) {
    if (!reg_str || strlen(reg_str) < 2) return 0;
    if (reg_str[0] == 'r' || reg_str[0] == 'R') {
        return atoi(&reg_str[1]);
    }
    return 0;
}

int parseImmediate(const char *imm_str) {
    if (!imm_str) return 0;
    for (int i = 0; i < label_count; i++) {
        if (strcmp(labels[i].name, imm_str) == 0) {
            return labels[i].address;
        }
    }
    return atoi(imm_str);
}

ProcessorInstruction* findInstruction(const char *mnemonic) {
    for (int i = 0; i < NUM_INSTRUCTIONS; i++) {
        if (strcasecmp(instructions[i].mnemonic, mnemonic) == 0) {
            return &instructions[i];
        }
    }
    return NULL;
}

uint32_t generateRType(ProcessorInstruction *instr, int rs, int rt, int rd, int shamt) {
    uint32_t binary = 0;
    binary |= ((uint32_t)instr->opcode & 0x3F) << 26;
    binary |= ((uint32_t)rs & 0x3F) << 20;
    binary |= ((uint32_t)rt & 0x3F) << 14;
    binary |= ((uint32_t)rd & 0x3F) << 8;
    binary |= ((uint32_t)shamt & 0xFF);
    return binary;
}

uint32_t generateIType(ProcessorInstruction *instr, int rs, int rt, int immediate) {
    uint32_t binary = 0;
    binary |= ((uint32_t)instr->opcode & 0x3F) << 26;
    binary |= ((uint32_t)rs & 0x3F) << 20;
    binary |= ((uint32_t)rt & 0x3F) << 14;
    binary |= ((uint32_t)immediate & 0x3FFF);
    return binary;
}

uint32_t generateJType(ProcessorInstruction *instr, int address) {
    uint32_t binary = 0;
    binary |= ((uint32_t)instr->opcode & 0x3F) << 26;
    binary |= ((uint32_t)address & 0x3F);
    return binary;
}

uint32_t parseInstruction(const char *line, uint32_t pc) {
    char instr_copy[256];
    strcpy(instr_copy, line);
    
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
    if (strstr(instr_copy, "Func") || strstr(instr_copy, "CEHOLDER") || 
        strstr(instr_copy, "Label")) {
        return 0xFFFFFFFF; // Special marker for non-instruction lines
    }
    
    // Skip single letter lines like "R" which are assembly artifacts
    if (strlen(instr_copy) == 1 || strlen(instr_copy) == 2) {
        return 0xFFFFFFFF; // Skip short non-instruction lines
    }
    
    char *tokens[8];
    int token_count = 0;
    
    char *token = strtok(instr_copy, " \t,");
    while (token && token_count < 8) {
        tokens[token_count++] = token;
        token = strtok(NULL, " \t,");
    }
    
    if (token_count == 0) return 0;
    
    ProcessorInstruction *instr = findInstruction(tokens[0]);
    if (!instr) return 0;
    
    switch (instr->format) {
        case FORMAT_R: {
            int rs = 0, rt = 0, rd = 0, shamt = 0;
            
            if (strcmp(instr->mnemonic, "add") == 0 || strcmp(instr->mnemonic, "sub") == 0 ||
                strcmp(instr->mnemonic, "and") == 0 || strcmp(instr->mnemonic, "or") == 0 ||
                strcmp(instr->mnemonic, "slt") == 0) {
                if (token_count >= 4) {
                    rd = parseRegister(tokens[1]);
                    rs = parseRegister(tokens[2]);
                    rt = parseRegister(tokens[3]);
                }
            } else if (strcmp(instr->mnemonic, "mult") == 0 || strcmp(instr->mnemonic, "div") == 0) {
                if (token_count >= 3) {
                    rs = parseRegister(tokens[1]);
                    rt = parseRegister(tokens[2]);
                }
            } else if (strcmp(instr->mnemonic, "move") == 0) {
                if (token_count >= 3) {
                    rd = parseRegister(tokens[1]);
                    rs = parseRegister(tokens[2]);
                }
            } else if (strcmp(instr->mnemonic, "mfhi") == 0 || strcmp(instr->mnemonic, "mflo") == 0) {
                if (token_count >= 2) {
                    rd = parseRegister(tokens[1]);
                }
            } else if (strcmp(instr->mnemonic, "jr") == 0 || strcmp(instr->mnemonic, "jalr") == 0) {
                if (token_count >= 2) {
                    rs = parseRegister(tokens[1]);
                }
            } else if (strcmp(instr->mnemonic, "sll") == 0 || strcmp(instr->mnemonic, "srl") == 0) {
                if (token_count >= 4) {
                    rd = parseRegister(tokens[1]);
                    rs = parseRegister(tokens[2]);
                    shamt = parseImmediate(tokens[3]);
                }
            } else if (strcmp(instr->mnemonic, "outputreg") == 0) {
                if (token_count >= 2) {
                    rs = parseRegister(tokens[1]);
                }
            } else if (strcmp(instr->mnemonic, "input") == 0) {
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
                if (token_count >= 4) {
                    rt = parseRegister(tokens[1]);
                    rs = parseRegister(tokens[2]);
                    immediate = parseImmediate(tokens[3]);
                }
            } else if (strcmp(instr->mnemonic, "li") == 0) {
                if (token_count >= 3) {
                    rt = parseRegister(tokens[1]);
                    immediate = parseImmediate(tokens[2]);
                }
            } else if (strstr(instr->mnemonic, "b") == instr->mnemonic) {
                if (token_count >= 4) {
                    rs = parseRegister(tokens[1]);
                    rt = parseRegister(tokens[2]);
                    immediate = parseImmediate(tokens[3]);
                }
            } else if (strcmp(instr->mnemonic, "lw") == 0 || strcmp(instr->mnemonic, "sw") == 0) {
                if (token_count >= 4) {
                    rt = parseRegister(tokens[1]);
                    rs = parseRegister(tokens[2]);
                    immediate = parseImmediate(tokens[3]);
                } else if (token_count >= 3) {
                    rt = parseRegister(tokens[1]);
                    rs = parseRegister(tokens[2]);
                    if (token_count > 3) {
                        immediate = parseImmediate(tokens[3]);
                    }
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

void collectLabels(FILE *asm_file) {
    char line[512];
    uint32_t pc = 0;
    
    while (fgets(line, sizeof(line), asm_file)) {
        char *newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        char *trimmed_line = line;
        while (*trimmed_line == ' ' || *trimmed_line == '\t') trimmed_line++;
        if (*trimmed_line == '\0') continue;
        
        if (strstr(trimmed_line, "Label") && strchr(trimmed_line, ':')) {
            char *label_start = strstr(trimmed_line, "Label");
            label_start += 6;
            while (*label_start == ' ') label_start++;
            
            char *colon = strchr(label_start, ':');
            if (colon) {
                *colon = '\0';
                strcpy(labels[label_count].name, label_start);
                labels[label_count].address = pc;
                label_count++;
            }
        }
        
        // Count actual instructions (including numbered comments as NOPs)
        char *dash = strchr(trimmed_line, '-');
        if (dash) {
            char *after_dash = dash + 1;
            while (*after_dash == ' ' || *after_dash == '\t') after_dash++;
            
            // If it's a numbered line, count it (even if it's a comment)
            if (after_dash[0] != '#' || (after_dash[0] == '#')) {
                // Skip only function headers and labels
                if (!strstr(after_dash, "Func") && !strstr(after_dash, "CEHOLDER") && 
                    !strstr(after_dash, "Label")) {
                    pc++;
                }
            }
        } else {
            // Non-numbered lines - only count if they're not comments/headers
            if (trimmed_line[0] != '#' && !strstr(trimmed_line, "Func") && 
                !strstr(trimmed_line, "CEHOLDER") && !strstr(trimmed_line, "Label")) {
                pc++;
            }
        }
    }
    
    rewind(asm_file);
}

void generateCleanBinary(const char *asm_filename, const char *bin_filename) {
    FILE *asm_file = fopen(asm_filename, "r");
    if (!asm_file) {
        printf("Error: Cannot open assembly file %s\n", asm_filename);
        return;
    }
    
    FILE *bin_file = fopen(bin_filename, "w");
    if (!bin_file) {
        printf("Error: Cannot create binary file %s\n", bin_filename);
        fclose(asm_file);
        return;
    }
    
    // Collect labels
    label_count = 0;
    collectLabels(asm_file);
    
    // Generate clean binary - just the binary numbers
    char line[512];
    uint32_t pc = 0;
    
    while (fgets(line, sizeof(line), asm_file)) {
        char *newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        if (strlen(line) == 0) continue;
        
        uint32_t binary = parseInstruction(line, pc);
        
        if (binary == 0xFFFFFFFF) {
            // Skip non-instruction lines (function headers, labels, "R" line)
            continue;
        } else if (binary == 0) {
            // Check if this is really a numbered comment or just a malformed line
            char *dash = strchr(line, '-');
            if (dash) {
                // This is a numbered line (even if comment) - should count as NOP
                for (int i = 31; i >= 0; i--) {
                    fprintf(bin_file, "0");
                }
                fprintf(bin_file, "\n");
                pc++;
            } else {
                // Non-numbered line that parsed as 0 - just skip
                continue;
            }
        } else {
            // Real instruction
            for (int i = 31; i >= 0; i--) {
                fprintf(bin_file, "%d", (binary >> i) & 1);
            }
            fprintf(bin_file, "\n");
            pc++;
        }
    }
    
    fclose(asm_file);
    fclose(bin_file);
    
    printf("Clean binary generation completed: %s\n", bin_filename);
    printf("Generated %u binary instructions\n", pc);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <assembly_file> <clean_binary_file>\n", argv[0]);
        printf("Example: %s gcd.asm gcd_clean.bin\n", argv[0]);
        return 1;
    }
    
    generateCleanBinary(argv[1], argv[2]);
    return 0;
}
