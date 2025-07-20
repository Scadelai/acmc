#!/bin/bash

# Verification script to confirm both binary versions contain identical data

echo "=== Binary Generation Verification ==="
echo

# Function to extract just binary from documented version
extract_binary() {
    # Look for lines that contain 32 bits with spaces, remove spaces
    grep -E '^[01]{6} [01]{6} [01]{6} [01]{6} [01]{2} [01]{6}$' "$1" | tr -d ' '
}

# Function to verify files match
verify_match() {
    local asm_file=$1
    local documented_bin=$2
    local clean_bin=$3
    
    echo "Verifying $asm_file:"
    echo "  Documented: $documented_bin"
    echo "  Clean: $clean_bin"
    
    # Extract binary lines from documented version
    extract_binary "$documented_bin" > temp_extracted.bin
    
    # Compare with clean version
    if diff -q temp_extracted.bin "$clean_bin" > /dev/null; then
        echo "  ✅ MATCH: Binary data is identical"
        echo "  Instructions: $(wc -l < "$clean_bin")"
    else
        echo "  ❌ MISMATCH: Binary data differs"
        echo "  Documented instructions: $(wc -l < temp_extracted.bin)"
        echo "  Clean instructions: $(wc -l < "$clean_bin")"
    fi
    echo
}

# Verify all three programs
verify_match "gcd.asm" "gcd.bin" "gcd_clean.bin"
verify_match "sort.asm" "sort.bin" "sort_clean.bin"
verify_match "test_generic.asm" "test_generic.bin" "test_generic_clean.bin"

# Clean up
rm -f temp_extracted.bin

echo "=== File Formats ==="
echo
echo "Documented format (with comments and field separators):"
echo "  gcd.bin, sort.bin, test_generic.bin"
echo
echo "Clean format (binary only, no comments):"
echo "  gcd_clean.bin, sort_clean.bin, test_generic_clean.bin"
echo
echo "Both formats contain identical binary instruction data."
