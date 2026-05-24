#!/usr/bin/env bash

# Exit on error
set -e

# Configuration
BUILD_DIR="build-cov"
AFL_QUEUE="fuzz_out/default/queue"
FUZZ_TARGET="./${BUILD_DIR}/tests/fuzz_allocator"

echo "[*] Checking for AFL++ corpus..."
if [ ! -d "${AFL_QUEUE}" ]; then
    echo "[!] Error: AFL++ queue directory not found (${AFL_QUEUE})."
    echo "    Please run the fuzzer first to generate a corpus."
    exit 1
fi

echo "[*] Setting up coverage build directory: ${BUILD_DIR}"
CC=clang \
meson setup "${BUILD_DIR}" --wipe \
    -Db_coverage=true \
    -Dbuildtype=debug

echo "[*] Building target with coverage instrumentation..."
ninja -C "${BUILD_DIR}" tests/fuzz_allocator

echo "[*] Running fuzzer corpus through instrumented binary..."
count=0
for file in "${AFL_QUEUE}"/id*; do
    [ -e "$file" ] || continue
    $FUZZ_TARGET "$file" > /dev/null 2>&1 || true
    count=$((count + 1))
done

echo "[+] Processed ${count} files from the corpus."

echo "[*] Attempting to generate HTML coverage report..."
if command -v gcovr &> /dev/null || (command -v lcov &> /dev/null && command -v genhtml &> /dev/null); then
    ninja -C "${BUILD_DIR}" coverage-html
    echo ""
    echo "[+] SUCCESS! HTML report generated at:"
    echo "    ${BUILD_DIR}/meson-logs/coveragereport/index.html"
else
    echo "[!] Warning: gcovr or lcov/genhtml not found."
    echo "[*] Generating a text-based summary instead using llvm-cov..."
    echo ""
    # Find the object file (meson names them differently)
    OBJ_FILE=$(find "${BUILD_DIR}" -name "fuzz_allocator.p" -type d | head -n 1)
    # This is a bit complex for a generic script, but let's try a simple gcov summary
    cd "${BUILD_DIR}"
    gcov -n tests/fuzz_allocator.p/*.o > /dev/null
    echo "--- Coverage Summary (Text) ---"
    grep -r "File" . --include "*.gcov" -A 1 | sed 's/File //;s/Lines executed://'
    echo "-------------------------------"
    echo ""
    echo "[?] To get a beautiful HTML visualization, please install 'gcovr' or 'lcov'."
fi
