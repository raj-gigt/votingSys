#!/bin/bash

# Compilation test script for enclave-collector
# This script compiles every .c file and reports errors

echo "=== Enclave Collector Compilation Test ==="
echo "Date: $(date)"
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Counters
total_files=0
success_count=0
error_count=0

# Create output directory for compilation artifacts
mkdir -p testbench/output
mkdir -p testbench/errors

# Common include paths
INCLUDES="-I./common -I./enclave -I./host -I./edl -I./common/bigint_lib"

# Common compiler flags
CFLAGS="-Wall -Wextra -std=c99 -DSIMULATION_MODE=1"

# Function to compile a single file
compile_file() {
    local file="$1"
    local output_name=$(basename "$file" .c)
    local output_file="testbench/output/${output_name}.o"
    local error_file="testbench/errors/${output_name}.err"
    
    echo -e "${BLUE}Compiling: $file${NC}"
    
    # Try to compile the file
    gcc $CFLAGS $INCLUDES -c "$file" -o "$output_file" 2> "$error_file"
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ SUCCESS: $file${NC}"
        success_count=$((success_count + 1))
        # Remove empty error file
        rm -f "$error_file"
    else
        echo -e "${RED}✗ FAILED: $file${NC}"
        echo -e "${YELLOW}Errors saved to: $error_file${NC}"
        error_count=$((error_count + 1))
        
        # Show first few lines of errors
        echo "--- Error Preview ---"
        head -10 "$error_file"
        echo "--- End Preview ---"
        echo ""
    fi
    
    total_files=$((total_files + 1))
}

echo "Starting compilation tests..."
echo ""

# Find and compile all .c files in the project
echo "=== ENCLAVE FILES ==="
for file in enclave/*.c; do
    if [ -f "$file" ]; then
        compile_file "$file"
    fi
done

echo ""
echo "=== HOST FILES ==="
for file in host/*.c; do
    if [ -f "$file" ]; then
        compile_file "$file"
    fi
done

echo ""
echo "=== COMMON FILES ==="
for file in common/*.c; do
    if [ -f "$file" ]; then
        compile_file "$file"
    fi
done

# Check bigint library if it exists
# if [ -d "common/bigint_lib" ]; then
#     echo ""
#     echo "=== BIGINT LIBRARY FILES ==="
#     for file in common/bigint_lib/*.c; do
#         if [ -f "$file" ]; then
#             compile_file "$file"
#         fi
#     done
# fi

# Summary
echo ""
echo "=== COMPILATION SUMMARY ==="
echo -e "Total files processed: ${BLUE}$total_files${NC}"
echo -e "Successful compilations: ${GREEN}$success_count${NC}"
echo -e "Failed compilations: ${RED}$error_count${NC}"

if [ $error_count -gt 0 ]; then
    echo ""
    echo -e "${YELLOW}=== ERROR DETAILS ===${NC}"
    echo "Error files saved in: testbench/errors/"
    echo "To view specific errors, check:"
    
    for error_file in testbench/errors/*.err; do
        if [ -f "$error_file" ]; then
            echo "  - $error_file"
        fi
    done
    
    echo ""
    echo -e "${RED}=== MOST COMMON ERRORS ===${NC}"
    echo "Scanning for common error patterns..."
    
    # Look for common error patterns
    if ls testbench/errors/*.err 1> /dev/null 2>&1; then
        echo ""
        echo "Missing includes/headers:"
        grep -h "fatal error.*No such file" testbench/errors/*.err 2>/dev/null | sort | uniq -c | sort -nr
        
        echo ""
        echo "Undefined references:"
        grep -h "undefined reference" testbench/errors/*.err 2>/dev/null | sort | uniq -c | sort -nr | head -10
        
        echo ""
        echo "Undeclared identifiers:"
        grep -h "undeclared" testbench/errors/*.err 2>/dev/null | sort | uniq -c | sort -nr | head -10
        
        echo ""
        echo "Type errors:"
        grep -h "conflicting types\|incompatible types" testbench/errors/*.err 2>/dev/null | sort | uniq -c | sort -nr | head -5
    fi
fi

echo ""
echo "=== RECOMMENDATIONS ==="
if [ $error_count -gt 0 ]; then
    echo "1. Fix missing header files first"
    echo "2. Ensure all type definitions are in shared_types.h"
    echo "3. Check function declarations in header files"
    echo "4. Verify include paths are correct"
    echo ""
    echo "To view all errors for a specific file:"
    echo "  cat testbench/errors/filename.err"
    echo ""
    echo "To clean up test files:"
    echo "  rm -rf testbench/"
else
    echo -e "${GREEN}All files compiled successfully!${NC}"
    echo "You can clean up with: rm -rf testbench/"
fi

echo ""
echo "=== TEST COMPLETE ==="
