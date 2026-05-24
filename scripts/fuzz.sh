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

# If a committed corpus exists, use it as the base
if [ -d "tests/fuzz_corpus" ] && [ "$(ls -A tests/fuzz_corpus)" ]; then
    echo "[*] Using committed corpus from tests/fuzz_corpus..."
    cp -r tests/fuzz_corpus/* "${IN_DIR}/"
fi

# Fallback/Additional structured seeds
# (These ensure we always have a good starting point even without a full corpus)
[ -f "${IN_DIR}/seed_c" ] || printf "\x00\x00\x00\x64\x04\x00\xC8\x02\x01\xF4\x05" > "${IN_DIR}/seed_c"
...
printf "\x04\x40\x00\x64\x00\x00\x40\x04\x00\x40\x01" > "${IN_DIR}/seed_pool"

# Add Buddy Seed
[ -f "${IN_DIR}/seed_buddy" ] || printf "\x05\x00" > "${IN_DIR}/seed_buddy"

echo "[+] Initialized corpus in ${IN_DIR}"

echo ""
echo "[+] Setup complete!"
echo "[*] To start fuzzing, run:"
echo "    AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 AFL_SKIP_CPUFREQ=1 afl-fuzz -i ${IN_DIR} -o ${OUT_DIR} -- ${FUZZ_TARGET} @@"
echo ""
echo "[?] Tip: You can also use '-' instead of '@@' if your fuzzer reads from stdin."
