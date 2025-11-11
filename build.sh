#!/bin/bash

# Archivas Core GUI Build Script for Linux

set -e

echo "Archivas Core GUI Build Script"
echo "==============================="
echo ""

# Check for Qt
if ! command -v qmake &> /dev/null && ! pkg-config --exists Qt6Core && ! pkg-config --exists Qt5Core; then
    echo "ERROR: Qt is not installed!"
    echo ""
    echo "Please install Qt first:"
    echo "  Ubuntu/Debian: sudo apt install qt6-base-dev cmake build-essential"
    echo "  Fedora: sudo dnf install qt6-qtbase-devel cmake gcc-c++"
    echo "  Arch: sudo pacman -S qt6-base cmake base-devel"
    echo ""
    exit 1
fi

# Check for CMake
if ! command -v cmake &> /dev/null; then
    echo "ERROR: CMake is not installed!"
    echo ""
    echo "Please install CMake first:"
    echo "  Ubuntu/Debian: sudo apt install cmake"
    echo "  Fedora: sudo dnf install cmake"
    echo "  Arch: sudo pacman -S cmake"
    echo ""
    exit 1
fi

# Create build directory
echo "Creating build directory..."
mkdir -p build
cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake ..

# Build
echo "Building application..."
make -j$(nproc)

# Check if build was successful
if [ -f "archivas-qt" ]; then
    echo ""
    echo "=========================================="
    echo "Build successful!"
    echo "=========================================="
    echo ""
    echo "The executable is located at:"
    echo "  $(pwd)/archivas-qt"
    echo ""
    echo "To run the application:"
    echo "  ./archivas-qt"
    echo ""
    echo "Or from the project root:"
    echo "  ./build/archivas-qt"
    echo ""
else
    echo ""
    echo "ERROR: Build failed! The executable was not created."
    echo "Please check the error messages above."
    echo ""
    exit 1
fi

