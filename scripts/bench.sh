#!/usr/bin/env bash

# Exit on error
set -e

# Project root directory
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build-bench"

# Configuration
CONFIDENCE="${1:-5}"

echo "===> Setting up benchmark build..."
if [ ! -d "${BUILD_DIR}" ]; then
    meson setup "${BUILD_DIR}" \
        -Db_sanitize=none \
        -Dbenchmarks=true \
        -Dbuildtype=release
else
    meson configure "${BUILD_DIR}" \
        -Db_sanitize=none \
        -Dbenchmarks=true \
        -Dbuildtype=release
fi

echo "===> Compiling benchmarks..."
meson compile -C "${BUILD_DIR}"

echo "===> Running benchmarks (Confidence Limit: ${CONFIDENCE}%)..."
# Using 'meson test --benchmark' to ensure sequential execution
meson test --benchmark -C "${BUILD_DIR}" --verbose --test-args="--confidence=${CONFIDENCE}"
