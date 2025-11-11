#!/bin/bash

# Build Release Version of Archivas Core GUI
# This creates an optimized release build

set -e

echo "Building Archivas Core GUI (Release)"
echo "===================================="
echo ""

# Clean previous release build
if [ -d "build-release" ]; then
    echo "Cleaning previous release build..."
    rm -rf build-release
fi

# Create build directory
mkdir -p build-release
cd build-release

# Configure with CMake (Release mode)
echo "Configuring CMake (Release mode)..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
echo ""
echo "Building application (this may take a while)..."
make -j$(nproc 2>/dev/null || echo 4)

# Check if build was successful
if [ -f "archivas-qt" ]; then
    echo ""
    echo "=========================================="
    echo "✓ Release build successful!"
    echo "=========================================="
    echo ""
    echo "Executable: $(pwd)/archivas-qt"
    echo "Size: $(du -h archivas-qt | cut -f1)"
    echo ""
    echo "To test the release build:"
    echo "  ./archivas-qt"
    echo ""
else
    echo ""
    echo "ERROR: Build failed! Executable not found."
    exit 1
fi

