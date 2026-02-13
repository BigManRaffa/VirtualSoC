#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INSTALL_DIR="${SCRIPT_DIR}/systemc-install"
SYSTEMC_VERSION="2.3.4"
SYSTEMC_URL="https://github.com/accellera-official/systemc/archive/refs/tags/${SYSTEMC_VERSION}.tar.gz"
TARBALL="${SCRIPT_DIR}/systemc-${SYSTEMC_VERSION}.tar.gz"
SRC_DIR="${SCRIPT_DIR}/systemc-${SYSTEMC_VERSION}"

echo "GamingCPU-VP Dependency Setup!"

# Check for C++17 support
if ! g++ -std=c++17 -x c++ -c /dev/null -o /dev/null 2>/dev/null; then
    echo "ERROR: g++ with C++17 support is required"
    exit 1
fi
echo "[OK] g++ with C++17 support found"

# Skip if its already installed
if [ -f "${INSTALL_DIR}/lib/libsystemc.a" ] || [ -f "${INSTALL_DIR}/lib-linux64/libsystemc.a" ]; then
    echo "[OK] SystemC already installed at ${INSTALL_DIR}"
    echo "     To reinstall, remove ${INSTALL_DIR}"
    export SYSTEMC_HOME="${INSTALL_DIR}"
    echo "export SYSTEMC_HOME=${INSTALL_DIR}"
    exit 0
fi

# Download SystemC!
if [ ! -f "${TARBALL}" ]; then
    echo "Downloading SystemC ${SYSTEMC_VERSION}..."
    curl -fSL -o "${TARBALL}" "${SYSTEMC_URL}"
else
    echo "[OK] SystemC tarball already cached"
fi

# Extract
if [ ! -d "${SRC_DIR}" ]; then
    echo "Extracting..."
    cd "${SCRIPT_DIR}"
    tar xzf "${TARBALL}"
fi

# Build SystemC with CMake
echo "Building SystemC ${SYSTEMC_VERSION}..."
BUILD_DIR="${SRC_DIR}/build"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

cmake .. \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
    -DCMAKE_CXX_STANDARD=17 \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DENABLE_PTHREADS=ON \
    2>&1

make -j"$(nproc)" 2>&1
make install 2>&1

echo ""
echo "SystemC ${SYSTEMC_VERSION} installed to ${INSTALL_DIR}"
echo "export SYSTEMC_HOME=${INSTALL_DIR}"
export SYSTEMC_HOME="${INSTALL_DIR}"

# Verify TLM headers exist
if [ -d "${INSTALL_DIR}/include/tlm" ] || [ -d "${INSTALL_DIR}/include/tlm_utils" ]; then
    echo "[OK] TLM-2.0 headers found"
else
    echo "[WARN] TLM headers not found in expected location, checking..."
    find "${INSTALL_DIR}" -name "tlm.h" 2>/dev/null || echo "TLM headers may need separate installation"
fi

echo ""
echo "Setup Complete"
echo "Add to your build: -DSYSTEMC_HOME=${INSTALL_DIR}"
