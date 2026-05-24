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
if [ ! "$(ls -A ${IN_DIR})" ]; then
    echo "seed" > "${IN_DIR}/seed"
    echo "[+] Created initial seed in ${IN_DIR}"
fi

echo ""
echo "[+] Setup complete!"
echo "[*] To start fuzzing, run:"
echo "    AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 AFL_SKIP_CPUFREQ=1 afl-fuzz -i ${IN_DIR} -o ${OUT_DIR} -- ${FUZZ_TARGET} @@"
echo ""
echo "[?] Tip: You can also use '-' instead of '@@' if your fuzzer reads from stdin."
