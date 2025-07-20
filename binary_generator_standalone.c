/*
 * binary_generator_standalone.c - Standalone binary generator tool
 * 
 * This is a standalone version of the binary generator that can be used
 * independently of the ACMC compiler to convert assembly files to binary.
 */

#include "binary_generator.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
    if (argc == 2) {
        // Auto-naming mode: generate both .bin and .binbd files
        generateBinaryWithAutoNaming(argv[1]);
    } else if (argc == 4) {
        // Manual naming mode: specify both output files
        generateBinaryFromAssembly(argv[1], argv[2], argv[3]);
    } else {
        printf("Usage: %s <assembly_file> [clean_binary_file] [commented_binary_file]\n", argv[0]);
        printf("Auto mode: %s gcd.asm (generates gcd.bin and gcd.binbd)\n", argv[0]);
        printf("Manual mode: %s gcd.asm gcd.bin gcd.binbd\n", argv[0]);
        return 1;
    }
    
    return 0;
}
