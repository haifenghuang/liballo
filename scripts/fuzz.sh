#!/usr/bin/env bash

# Exit on error
set -e

# Configuration
BUILD_DIR="build-afl"
IN_DIR="fuzz_in"
OUT_DIR="fuzz_out"
FUZZ_TARGET="./${BUILD_DIR}/tests/fuzz_allocator"

echo "[*] Checking for AFL++ compilers..."
if ! command -v afl-clang-lto &> /dev/null; then
    echo "[!] Error: afl-clang-lto not found. Please install AFL++."
    exit 1
fi

echo "[*] Setting up build directory: ${BUILD_DIR}"
CC=afl-clang-lto CXX=afl-clang-lto++ \
meson setup "${BUILD_DIR}" --wipe \
    -Db_sanitize=address,undefined \
    -Db_lundef=false \
    -Dbuildtype=debugoptimized

echo "[*] Building fuzz target..."
ninja -C "${BUILD_DIR}" tests/fuzz_allocator

echo "[*] Preparing input corpus..."
mkdir -p "${IN_DIR}"

# Function to create a seed: selector, then a sequence of (op_byte, size_high, size_low)
# Op byte: (index << 2) | op
# OP_ALLOC=0, OP_FREE=1, OP_REALLOC=2

# 1. C Allocator Seed (Selector 0)
# Alloc(idx 0, 100), Alloc(idx 1, 200), Realloc(idx 0, 500), Free(idx 1)
printf "\x00\x00\x00\x64\x04\x00\xC8\x02\x01\xF4\x05" > "${IN_DIR}/seed_c"

# 2. Page Allocator Seed (Selector 1)
# Alloc(idx 0, 4096), Alloc(idx 1, 8192), Free(idx 0)
printf "\x01\x00\x10\x00\x04\x20\x00\x01" > "${IN_DIR}/seed_page"

# 3. Arena Allocator Seed (Selector 3)
# BlockSize=4096 (0x1000). Alloc(idx 0, 100), Alloc(idx 1, 4000) -> triggers new block
printf "\x03\x10\x00\x00\x00\x64\x04\x0F\xA0" > "${IN_DIR}/seed_arena"

# 4. Pool Allocator Seed (Selector 4)
# PoolBlockSize=64, TotalBlocks=100. Alloc(idx 0, 64), Alloc(idx 1, 64), Free(idx 0)
printf "\x04\x40\x00\x64\x00\x00\x40\x04\x00\x40\x01" > "${IN_DIR}/seed_pool"

echo "[+] Created structured seeds in ${IN_DIR}"

echo ""
echo "[+] Setup complete!"
echo "[*] To start fuzzing, run:"
echo "    AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 AFL_SKIP_CPUFREQ=1 afl-fuzz -i ${IN_DIR} -o ${OUT_DIR} -- ${FUZZ_TARGET} @@"
echo ""
echo "[?] Tip: You can also use '-' instead of '@@' if your fuzzer reads from stdin."
