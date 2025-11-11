#!/bin/bash

# Fix incomplete package installation
# Run this if the initial installation had repository errors

set -e

echo "Fixing incomplete package installation..."
echo "=========================================="
echo ""

# Retry installing the failed packages
echo "Retrying installation of failed packages..."
sudo apt install -y --fix-missing libxcb1-dev libxext-dev

echo ""
echo "Completing Qt5 and CMake installation..."
sudo apt install -y --fix-missing qtbase5-dev qtbase5-dev-tools cmake build-essential

echo ""
echo "Verifying installation..."
if command -v cmake &> /dev/null; then
    echo "✓ CMake installed: $(cmake --version | head -1)"
else
    echo "✗ CMake not found"
    exit 1
fi

if command -v qmake &> /dev/null || pkg-config --exists Qt5Core 2>/dev/null; then
    echo "✓ Qt5 installed"
    if command -v qmake &> /dev/null; then
        echo "  $(qmake --version | head -1)"
    fi
else
    echo "✗ Qt5 not found"
    exit 1
fi

echo ""
echo "=========================================="
echo "Installation complete! You can now build:"
echo "  ./setup-and-build.sh"
echo "=========================================="

