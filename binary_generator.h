#ifndef BINARY_GENERATOR_H
#define BINARY_GENERATOR_H

/**
 * binary_generator.h - Binary code generation for ACMC compiler
 * 
 * Provides functions to convert assembly code to binary representation
 * following the custom MIPS processor specification.
 */

// Generate both clean (.bin) and commented (.binbd) binary files from assembly
void generateBinaryWithAutoNaming(const char *base_filename);

// Generate binary files with specific filenames
void generateBinaryFromAssembly(const char *asm_filename, const char *clean_bin_filename, const char *commented_bin_filename);

#endif /* BINARY_GENERATOR_H */
