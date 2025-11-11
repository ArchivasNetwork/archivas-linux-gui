#!/bin/bash

# Complete setup and build script for Archivas Core GUI
# This script will guide you through the process

set -e

echo "=========================================="
echo "Archivas Core GUI - Setup and Build"
echo "=========================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Check for required commands
echo "Checking for build tools..."

MISSING_DEPS=()

if ! command_exists cmake; then
    echo -e "${RED}✗ CMake not found${NC}"
    MISSING_DEPS+=("cmake")
else
    echo -e "${GREEN}✓ CMake found${NC}"
    cmake --version | head -1
fi

if ! command_exists qmake && ! pkg-config --exists Qt5Core 2>/dev/null && ! pkg-config --exists Qt6Core 2>/dev/null; then
    echo -e "${RED}✗ Qt development packages not found${NC}"
    MISSING_DEPS+=("qtbase5-dev")
else
    echo -e "${GREEN}✓ Qt found${NC}"
    if command_exists qmake; then
        qmake --version | head -1
    fi
fi

if ! command_exists make; then
    echo -e "${RED}✗ Make not found${NC}"
    MISSING_DEPS+=("build-essential")
else
    echo -e "${GREEN}✓ Make found${NC}"
fi

echo ""

# If dependencies are missing, provide installation instructions
if [ ${#MISSING_DEPS[@]} -ne 0 ]; then
    echo -e "${YELLOW}Missing dependencies detected!${NC}"
    echo ""
    echo "To install missing dependencies, run:"
    echo ""
    echo -e "${GREEN}sudo apt update${NC}"
    echo -e "${GREEN}sudo apt install -y ${MISSING_DEPS[*]} qtbase5-dev-tools${NC}"
    echo ""
    echo "Or use the install script:"
    echo -e "${GREEN}sudo ./install-deps.sh${NC}"
    echo ""
    echo "After installing dependencies, run this script again:"
    echo -e "${GREEN}./setup-and-build.sh${NC}"
    echo ""
    exit 1
fi

# All dependencies are present, proceed with build
echo -e "${GREEN}All dependencies found! Proceeding with build...${NC}"
echo ""

# Create build directory
echo "Creating build directory..."
mkdir -p build
cd build

# Configure with CMake
echo "Configuring with CMake..."
if cmake ..; then
    echo -e "${GREEN}✓ CMake configuration successful${NC}"
else
    echo -e "${RED}✗ CMake configuration failed${NC}"
    echo ""
    echo "Trying with explicit Qt5 path..."
    cmake .. -DCMAKE_PREFIX_PATH=/usr/lib/x86_64-linux-gnu/qt5 || {
        echo -e "${RED}CMake configuration failed. Please check Qt installation.${NC}"
        exit 1
    }
fi

echo ""

# Build
echo "Building application..."
if make -j$(nproc 2>/dev/null || echo 4); then
    echo ""
    echo "=========================================="
    echo -e "${GREEN}Build successful!${NC}"
    echo "=========================================="
    echo ""
    if [ -f "archivas-qt" ]; then
        echo -e "${GREEN}Executable created:${NC} $(pwd)/archivas-qt"
        echo ""
        echo "To run the application:"
        echo -e "${GREEN}./archivas-qt${NC}"
        echo ""
        echo "Or from the project root:"
        echo -e "${GREEN}./build/archivas-qt${NC}"
        echo ""
        
        # Show file info
        ls -lh archivas-qt
    else
        echo -e "${RED}ERROR: Build completed but executable not found!${NC}"
        exit 1
    fi
else
    echo ""
    echo -e "${RED}Build failed! Please check the error messages above.${NC}"
    exit 1
fi

