#!/bin/bash
# Build OxoSynth2 VST3 for Linux
# Usage: ./build_linux.sh [--no-install] [--bench] [--debug]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/Linux_Build/build"
INSTALL_DIR="${HOME}/.vst3"
NO_INSTALL=false
RUN_BENCH=false
BUILD_TYPE="Release"

for arg in "$@"; do
    case $arg in
        --no-install) NO_INSTALL=true ;;
        --bench) RUN_BENCH=true ;;
        --debug) BUILD_TYPE="Debug" ;;
        *) echo "Unknown option: $arg"; exit 1 ;;
    esac
done

echo "Building OxoSynth2 VST3 for Linux (${BUILD_TYPE})..."

# Check for required dependencies
check_dep() {
    if ! pkg-config --exists "$1"; then
        echo "Missing dependency: $1"
        echo "Install with: sudo apt-get install $2"
        exit 1
    fi
}

check_dep x11 "libx11-dev"
check_dep cairo "libcairo2-dev"
check_dep libcurl "libcurl4-openssl-dev"
check_dep zlib "zlib1g-dev"

# Check for VST3 SDK submodule
if [ ! -f "${SCRIPT_DIR}/third_party/vst3sdk/CMakeLists.txt" ]; then
    echo "VST3 SDK submodule not found. Running git submodule update..."
    git submodule update --init --recursive
fi

# Configure and build
cmake -S "${SCRIPT_DIR}/Linux_Build" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_CXX_STANDARD=20 \
    -DCMAKE_CXX_STANDARD_REQUIRED=ON

cmake --build "${BUILD_DIR}" --config "${BUILD_TYPE}" --parallel $(nproc)

# Run bench if requested
if [ "$RUN_BENCH" = true ]; then
    echo "Running CPU bench..."
    "${BUILD_DIR}/synth2_bench"
fi

# Install
if [ "$NO_INSTALL" = false ]; then
    echo "Installing to ${INSTALL_DIR}..."
    mkdir -p "${INSTALL_DIR}"
    cp -r "${BUILD_DIR}/VST3/Release/OxoSynth2.vst3" "${INSTALL_DIR}/"
    echo "Installed OxoSynth2.vst3 to ${INSTALL_DIR}"
fi

echo "Build complete!"