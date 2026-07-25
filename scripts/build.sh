#!/usr/bin/env bash
# Build helper for the C++ core (Phase 1: just src/core).
set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

info()  { echo -e "${GREEN}==>${NC} $*"; }
warn()  { echo -e "${YELLOW}==>${NC} $*"; }
error() { echo -e "${RED}==>${NC} $*" >&2; }

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
CLEAN=0
BUILD_TYPE="RelWithDebInfo"
JOBS="$(nproc)"

usage() {
    echo "Usage: $0 [--clean] [--debug] [-j N]"
    echo "  --clean   remove the build directory before configuring"
    echo "  --debug   configure a Debug build instead of RelWithDebInfo"
    echo "  -j N      parallel build jobs (default: nproc = ${JOBS})"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --clean) CLEAN=1; shift ;;
        --debug) BUILD_TYPE="Debug"; shift ;;
        -j) JOBS="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) error "Unknown option: $1"; usage; exit 1 ;;
    esac
done

for tool in cmake make; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        error "Required tool '$tool' not found. Install it and try again."
        exit 1
    fi
done

if [[ "$CLEAN" -eq 1 && -d "$BUILD_DIR" ]]; then
    warn "Removing existing build directory"
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"

info "Configuring (build type: ${BUILD_TYPE})"
if ! cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"; then
    error "CMake configuration failed"
    exit 1
fi

info "Building with ${JOBS} job(s)"
if ! cmake --build "$BUILD_DIR" -j "$JOBS"; then
    error "Build failed"
    exit 1
fi

info "Build succeeded: ${BUILD_DIR}"
